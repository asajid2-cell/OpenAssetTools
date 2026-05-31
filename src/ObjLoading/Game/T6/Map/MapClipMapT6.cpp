#include "MapClipMapT6.h"

#include "Utils/Logging/Log.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

using namespace T6;

namespace
{
    constexpr auto MAX_COLLISION_VERTS = std::numeric_limits<std::uint16_t>::max();
    constexpr auto MAX_AABB_TREE_CHILDREN = 128u;
    constexpr auto MAX_NODE_SIZE = 512.0f;
    constexpr auto MATERIAL_SURFACE_FLAGS = 278656;
    constexpr auto MATERIAL_CONTENT_FLAGS = 134217728;
    constexpr auto LEAF_TERRAIN_CONTENTS = 1;
    constexpr auto GENERATED_LEAF_BRUSH_CONTENTS = 134414848;
    constexpr auto TERMINAL_LEAF_BRUSH_AXIS = 0;
    constexpr auto GENERATED_LEAF_BRUSH_NODE = 1;
    constexpr auto DEFAULT_DYN_ENTITY_COUNT = 256u;
    constexpr auto COLLISION_BOUNDS_EPSILON = 0.125f;
    constexpr auto GENERATED_VISIBILITY_BYTES = 136;
    constexpr const char* DEFAULT_CLIP_MATERIAL = "light_demote_hint";

    enum class PlaneAxis : std::uint8_t
    {
        X,
        Y,
        Z,
    };

    enum class PlaneSide : std::uint8_t
    {
        Front,
        Back,
        Intersects,
    };

    struct TreeObject
    {
        vec3_t m_mins;
        vec3_t m_maxs;
        int m_partition_index;
    };

    class TreeLeaf
    {
    public:
        void AddObject(std::shared_ptr<TreeObject> object)
        {
            m_objects.emplace_back(std::move(object));
        }

        [[nodiscard]] TreeObject* GetObject(const std::size_t index) const
        {
            return m_objects.at(index).get();
        }

        [[nodiscard]] std::size_t GetObjectCount() const
        {
            return m_objects.size();
        }

    private:
        std::vector<std::shared_ptr<TreeObject>> m_objects;
    };

    class Tree;

    class TreeNode
    {
    public:
        TreeNode(std::unique_ptr<Tree> front, std::unique_ptr<Tree> back, const PlaneAxis axis, const float distance)
            : m_front(std::move(front)),
              m_back(std::move(back)),
              m_axis(axis),
              m_distance(distance)
        {
        }

        [[nodiscard]] PlaneSide ObjectSide(const TreeObject& object) const
        {
            float minCoord;
            float maxCoord;

            if (m_axis == PlaneAxis::X)
            {
                minCoord = object.m_mins.x;
                maxCoord = object.m_maxs.x;
            }
            else if (m_axis == PlaneAxis::Y)
            {
                minCoord = object.m_mins.y;
                maxCoord = object.m_maxs.y;
            }
            else
            {
                minCoord = object.m_mins.z;
                maxCoord = object.m_maxs.z;
            }

            if (maxCoord < m_distance)
                return PlaneSide::Back;

            if (minCoord > m_distance)
                return PlaneSide::Front;

            return PlaneSide::Intersects;
        }

        std::unique_ptr<Tree> m_front;
        std::unique_ptr<Tree> m_back;
        PlaneAxis m_axis;
        float m_distance;
    };

    class Tree
    {
    public:
        Tree(const vec3_t mins, const vec3_t maxs, const int level)
            : m_mins(mins),
              m_maxs(maxs),
              m_level(level)
        {
            Split();
        }

        void AddObject(std::shared_ptr<TreeObject> object) const
        {
            if (m_is_leaf)
            {
                m_leaf->AddObject(std::move(object));
                return;
            }

            const auto side = m_node->ObjectSide(*object);
            if (side == PlaneSide::Front)
                m_node->m_front->AddObject(std::move(object));
            else if (side == PlaneSide::Back)
                m_node->m_back->AddObject(std::move(object));
            else
            {
                m_node->m_front->AddObject(object);
                m_node->m_back->AddObject(std::move(object));
            }
        }

        bool m_is_leaf = true;
        std::unique_ptr<TreeLeaf> m_leaf;
        std::unique_ptr<TreeNode> m_node;
        vec3_t m_mins;
        vec3_t m_maxs;
        int m_level;

    private:
        void Split()
        {
            const auto xSize = m_maxs.x - m_mins.x;
            const auto ySize = m_maxs.y - m_mins.y;
            const auto zSize = m_maxs.z - m_mins.z;

            const auto shouldForceRootSplit = m_level == 0 && std::max({xSize, ySize, zSize}) > 0.0f;
            if (xSize > MAX_NODE_SIZE || (shouldForceRootSplit && xSize >= ySize && xSize >= zSize))
            {
                const auto half = (m_mins.x + m_maxs.x) * 0.5f;
                auto frontMins = m_mins;
                frontMins.x = half;
                auto backMaxs = m_maxs;
                backMaxs.x = half;
                m_node = std::make_unique<TreeNode>(std::make_unique<Tree>(frontMins, m_maxs, m_level + 1),
                                                    std::make_unique<Tree>(m_mins, backMaxs, m_level + 1),
                                                    PlaneAxis::X,
                                                    half);
                m_is_leaf = false;
                return;
            }

            if (ySize > MAX_NODE_SIZE || (shouldForceRootSplit && ySize >= xSize && ySize >= zSize))
            {
                const auto half = (m_mins.y + m_maxs.y) * 0.5f;
                auto frontMins = m_mins;
                frontMins.y = half;
                auto backMaxs = m_maxs;
                backMaxs.y = half;
                m_node = std::make_unique<TreeNode>(std::make_unique<Tree>(frontMins, m_maxs, m_level + 1),
                                                    std::make_unique<Tree>(m_mins, backMaxs, m_level + 1),
                                                    PlaneAxis::Y,
                                                    half);
                m_is_leaf = false;
                return;
            }

            if (zSize > MAX_NODE_SIZE || shouldForceRootSplit)
            {
                const auto half = (m_mins.z + m_maxs.z) * 0.5f;
                auto frontMins = m_mins;
                frontMins.z = half;
                auto backMaxs = m_maxs;
                backMaxs.z = half;
                m_node = std::make_unique<TreeNode>(std::make_unique<Tree>(frontMins, m_maxs, m_level + 1),
                                                    std::make_unique<Tree>(m_mins, backMaxs, m_level + 1),
                                                    PlaneAxis::Z,
                                                    half);
                m_is_leaf = false;
                return;
            }

            m_is_leaf = true;
            m_leaf = std::make_unique<TreeLeaf>();
        }
    };

    struct ClipBuildState
    {
        std::vector<cplane_s> m_planes;
        std::vector<cNode_t> m_nodes;
        std::vector<cLeaf_s> m_leafs;
        std::vector<CollisionAabbTree> m_aabb_trees;
    };

    template<typename T> T* AllocZeroed(MemoryManager& memory, const std::size_t count = 1u)
    {
        auto* result = memory.Alloc<T>(count);
        std::memset(result, 0, sizeof(T) * count);
        return result;
    }

    void UpdateAabbWithPoint(const vec3_t& point, vec3_t& mins, vec3_t& maxs)
    {
        mins.x = std::min(mins.x, point.x);
        mins.y = std::min(mins.y, point.y);
        mins.z = std::min(mins.z, point.z);
        maxs.x = std::max(maxs.x, point.x);
        maxs.y = std::max(maxs.y, point.y);
        maxs.z = std::max(maxs.z, point.z);
    }

    void ExpandBounds(vec3_t& mins, vec3_t& maxs, const float amount)
    {
        mins.x -= amount;
        mins.y -= amount;
        mins.z -= amount;
        maxs.x += amount;
        maxs.y += amount;
        maxs.z += amount;
    }

    [[nodiscard]] vec3_t CalcMiddleOfAabb(const vec3_t& mins, const vec3_t& maxs)
    {
        vec3_t result;
        result.x = (mins.x + maxs.x) * 0.5f;
        result.y = (mins.y + maxs.y) * 0.5f;
        result.z = (mins.z + maxs.z) * 0.5f;
        return result;
    }

    [[nodiscard]] vec3_t CalcHalfSizeOfAabb(const vec3_t& mins, const vec3_t& maxs)
    {
        vec3_t result;
        result.x = (maxs.x - mins.x) * 0.5f;
        result.y = (maxs.y - mins.y) * 0.5f;
        result.z = (maxs.z - mins.z) * 0.5f;
        return result;
    }

    [[nodiscard]] float DistBetweenPoints(const vec3_t& p1, const vec3_t& p2)
    {
        const auto x = p2.x - p1.x;
        const auto y = p2.y - p1.y;
        const auto z = p2.z - p1.z;
        return std::sqrt((x * x) + (y * y) + (z * z));
    }

    void LoadDynEnts(MemoryManager& memory, clipMap_t& clipMap)
    {
        clipMap.originalDynEntCount = 0u;
        clipMap.dynEntCount[0] = DEFAULT_DYN_ENTITY_COUNT;
        clipMap.dynEntCount[1] = 0u;
        clipMap.dynEntCount[2] = 0u;
        clipMap.dynEntCount[3] = 0u;

        clipMap.dynEntClientList[0] = clipMap.dynEntCount[0] ? AllocZeroed<DynEntityClient>(memory, clipMap.dynEntCount[0]) : nullptr;
        clipMap.dynEntClientList[1] = nullptr;
        clipMap.dynEntServerList[0] = clipMap.dynEntCount[2] ? AllocZeroed<DynEntityServer>(memory, clipMap.dynEntCount[2]) : nullptr;
        clipMap.dynEntServerList[1] = clipMap.dynEntCount[3] ? AllocZeroed<DynEntityServer>(memory, clipMap.dynEntCount[3]) : nullptr;
        clipMap.dynEntCollList[0] = clipMap.dynEntCount[0] ? AllocZeroed<DynEntityColl>(memory, clipMap.dynEntCount[0]) : nullptr;
        clipMap.dynEntCollList[1] = nullptr;
        clipMap.dynEntCollList[2] = nullptr;
        clipMap.dynEntCollList[3] = nullptr;
        clipMap.dynEntPoseList[0] = clipMap.dynEntCount[0] ? AllocZeroed<DynEntityPose>(memory, clipMap.dynEntCount[0]) : nullptr;
        clipMap.dynEntPoseList[1] = nullptr;
        clipMap.dynEntDefList[0] = clipMap.dynEntCount[0] ? AllocZeroed<DynEntityDef>(memory, clipMap.dynEntCount[0]) : nullptr;
        clipMap.dynEntDefList[1] = nullptr;
    }

    void LoadVisibility(MemoryManager& memory, clipMap_t& clipMap)
    {
        clipMap.numClusters = 1;
        clipMap.vised = 0;
        clipMap.clusterBytes = GENERATED_VISIBILITY_BYTES;
        clipMap.visibility = memory.Alloc<char>(clipMap.clusterBytes);
        std::memset(clipMap.visibility, 0xFF, clipMap.clusterBytes);
    }

    void LoadBoxData(clipMap_t& clipMap)
    {
        constexpr auto boxMins = 0x7F7FFFFFu;
        constexpr auto boxMaxs = 0xFF7FFFFFu;
        *(reinterpret_cast<unsigned int*>(&clipMap.box_model.leaf.mins.x)) = boxMins;
        *(reinterpret_cast<unsigned int*>(&clipMap.box_model.leaf.mins.y)) = boxMins;
        *(reinterpret_cast<unsigned int*>(&clipMap.box_model.leaf.mins.z)) = boxMins;
        *(reinterpret_cast<unsigned int*>(&clipMap.box_model.leaf.maxs.x)) = boxMaxs;
        *(reinterpret_cast<unsigned int*>(&clipMap.box_model.leaf.maxs.y)) = boxMaxs;
        *(reinterpret_cast<unsigned int*>(&clipMap.box_model.leaf.maxs.z)) = boxMaxs;

        clipMap.box_model.leaf.brushContents = -1;
        clipMap.box_model.leaf.terrainContents = 0;
        clipMap.box_model.leaf.cluster = 0;
        clipMap.box_model.leaf.collAabbCount = 0;
        clipMap.box_model.leaf.firstCollAabbIndex = 0;
        clipMap.box_model.leaf.leafBrushNode = 0;
        clipMap.box_model.mins = {};
        clipMap.box_model.maxs = {};
        clipMap.box_model.radius = 0.0f;
        clipMap.box_model.info = nullptr;
        clipMap.box_brush = nullptr;
    }

    void LoadRopesAndConstraints(MemoryManager& memory, clipMap_t& clipMap)
    {
        clipMap.num_constraints = 0;
        clipMap.constraints = nullptr;
        clipMap.max_ropes = 32;
        clipMap.ropes = AllocZeroed<rope_t>(memory, clipMap.max_ropes);
    }

    void LoadSubModelCollision(MemoryManager& memory, clipMap_t& clipMap, const GfxWorld& gfxWorld)
    {
        assert(gfxWorld.modelCount == 1);

        clipMap.numSubModels = 1u;
        clipMap.cmodels = AllocZeroed<cmodel_t>(memory, 1u);

        const auto& gfxModel = gfxWorld.models[0];
        clipMap.cmodels[0].mins = gfxModel.bounds[0];
        clipMap.cmodels[0].maxs = gfxModel.bounds[1];
        clipMap.cmodels[0].radius = DistBetweenPoints(clipMap.cmodels[0].mins, clipMap.cmodels[0].maxs) * 0.5f;
        clipMap.cmodels[0].leaf.firstCollAabbIndex = 0;
        clipMap.cmodels[0].leaf.collAabbCount = 0;
        clipMap.cmodels[0].leaf.brushContents = 0;
        clipMap.cmodels[0].leaf.terrainContents = 0;
        clipMap.cmodels[0].leaf.mins = {};
        clipMap.cmodels[0].leaf.maxs = {};
        clipMap.cmodels[0].leaf.leafBrushNode = 0;
        clipMap.cmodels[0].leaf.cluster = 0;
        clipMap.cmodels[0].info = nullptr;
    }

    void LoadXModelCollision(clipMap_t& clipMap)
    {
        clipMap.numStaticModels = 0u;
        clipMap.staticModelList = nullptr;
    }

    void FinalizeBoxModelLeafBrushNode(clipMap_t& clipMap)
    {
        if (clipMap.info.leafbrushNodesCount == 0u)
            return;

        // Stock T6 maps point the synthetic box model at the trailing leaf-brush
        // node instead of the root node. Keep generated clipmaps on that contract
        // so runtime box traces do not walk an uninitialized root as a leaf.
        clipMap.box_model.leaf.leafBrushNode = static_cast<int>(clipMap.info.leafbrushNodesCount - 1u);
    }

    void FinalizeBoxBrush(MemoryManager& memory, clipMap_t& clipMap)
    {
        clipMap.box_brush = AllocZeroed<cbrush_t>(memory);
        clipMap.box_brush->contents = -1;
        for (auto side = 0u; side < 2u; side++)
        {
            for (auto axis = 0u; axis < 3u; axis++)
            {
                clipMap.box_brush->axial_cflags[side][axis] = -1;
                clipMap.box_brush->axial_sflags[side][axis] = -1;
            }
        }
    }

    void AddAabbTreeFromLeaf(MemoryManager& memory, clipMap_t& clipMap, ClipBuildState& state, const Tree& tree, std::size_t& parentCount, std::size_t& parentStartIndex)
    {
        assert(tree.m_is_leaf);

        const auto leafObjectCount = tree.m_leaf->GetObjectCount();
        assert(leafObjectCount > 0u);

        parentCount = leafObjectCount / MAX_AABB_TREE_CHILDREN;
        if (leafObjectCount % MAX_AABB_TREE_CHILDREN > 0u)
            parentCount++;

        parentStartIndex = state.m_aabb_trees.size();
        state.m_aabb_trees.resize(state.m_aabb_trees.size() + parentCount);

        auto remainingObjectCount = leafObjectCount;
        auto addedObjectCount = 0u;
        for (auto parentIndex = 0u; parentIndex < parentCount; parentIndex++)
        {
            auto childObjectCount = MAX_AABB_TREE_CHILDREN;
            if (remainingObjectCount <= MAX_AABB_TREE_CHILDREN)
                childObjectCount = static_cast<unsigned>(remainingObjectCount);
            else
                remainingObjectCount -= MAX_AABB_TREE_CHILDREN;

            vec3_t parentMins;
            vec3_t parentMaxs;
            auto hasParentBounds = false;

            for (auto objectIndex = 0u; objectIndex < childObjectCount; objectIndex++)
            {
                const auto partitionIndex = tree.m_leaf->GetObject(addedObjectCount + objectIndex)->m_partition_index;
                const auto& partition = clipMap.partitions[partitionIndex];
                for (auto uindIndex = 0; uindIndex < partition.nuinds; uindIndex++)
                {
                    const auto vertex = clipMap.verts[clipMap.info.uinds[partition.fuind + uindIndex]];
                    if (!hasParentBounds)
                    {
                        parentMins = vertex;
                        parentMaxs = vertex;
                        hasParentBounds = true;
                    }
                    UpdateAabbWithPoint(vertex, parentMins, parentMaxs);
                }
            }

            ExpandBounds(parentMins, parentMaxs, COLLISION_BOUNDS_EPSILON);

            const auto childStartIndex = state.m_aabb_trees.size();
            CollisionAabbTree parentAabb{};
            parentAabb.origin = CalcMiddleOfAabb(parentMins, parentMaxs);
            parentAabb.halfSize = CalcHalfSizeOfAabb(parentMins, parentMaxs);
            parentAabb.materialIndex = 0u;
            parentAabb.childCount = static_cast<std::uint16_t>(childObjectCount);
            parentAabb.u.firstChildIndex = static_cast<int>(childStartIndex);
            state.m_aabb_trees[parentStartIndex + parentIndex] = parentAabb;

            for (auto objectIndex = 0u; objectIndex < childObjectCount; objectIndex++)
            {
                const auto partitionIndex = tree.m_leaf->GetObject(addedObjectCount + objectIndex)->m_partition_index;
                const auto& partition = clipMap.partitions[partitionIndex];

                vec3_t childMins;
                vec3_t childMaxs;
                auto hasChildBounds = false;
                for (auto uindIndex = 0; uindIndex < partition.nuinds; uindIndex++)
                {
                    const auto vertex = clipMap.verts[clipMap.info.uinds[partition.fuind + uindIndex]];
                    if (!hasChildBounds)
                    {
                        childMins = vertex;
                        childMaxs = vertex;
                        hasChildBounds = true;
                    }
                    UpdateAabbWithPoint(vertex, childMins, childMaxs);
                }

                ExpandBounds(childMins, childMaxs, COLLISION_BOUNDS_EPSILON);

                CollisionAabbTree childAabb{};
                childAabb.origin = CalcMiddleOfAabb(childMins, childMaxs);
                childAabb.halfSize = CalcHalfSizeOfAabb(childMins, childMaxs);
                childAabb.materialIndex = 0u;
                childAabb.childCount = 0u;
                childAabb.u.partitionIndex = partitionIndex;
                state.m_aabb_trees.emplace_back(childAabb);
            }

            addedObjectCount += childObjectCount;
        }
    }

    int16_t LoadTreeNode(MemoryManager& memory, clipMap_t& clipMap, ClipBuildState& state, const Tree& tree)
    {
        if (tree.m_is_leaf)
        {
            cLeaf_s leaf{};
            leaf.cluster = 0;
            leaf.brushContents = GENERATED_LEAF_BRUSH_CONTENTS;
            leaf.terrainContents = LEAF_TERRAIN_CONTENTS;
            leaf.mins = tree.m_mins;
            leaf.maxs = tree.m_maxs;
            ExpandBounds(leaf.mins, leaf.maxs, COLLISION_BOUNDS_EPSILON);
            leaf.leafBrushNode = GENERATED_LEAF_BRUSH_NODE;

            if (tree.m_leaf->GetObjectCount() > 0u)
            {
                std::size_t parentCount = 0u;
                std::size_t parentStartIndex = 0u;
                AddAabbTreeFromLeaf(memory, clipMap, state, tree, parentCount, parentStartIndex);
                leaf.collAabbCount = static_cast<std::uint16_t>(parentCount);
                leaf.firstCollAabbIndex = static_cast<std::uint16_t>(parentStartIndex);
            }

            const auto leafIndex = static_cast<std::uint16_t>(state.m_leafs.size());
            state.m_leafs.emplace_back(leaf);
            return static_cast<int16_t>(-1 - leafIndex);
        }

        cplane_s plane{};
        plane.dist = tree.m_node->m_distance;
        if (tree.m_node->m_axis == PlaneAxis::X)
        {
            plane.normal.x = 1.0f;
            plane.type = 0;
        }
        else if (tree.m_node->m_axis == PlaneAxis::Y)
        {
            plane.normal.y = 1.0f;
            plane.type = 1;
        }
        else
        {
            plane.normal.z = 1.0f;
            plane.type = 2;
        }
        state.m_planes.emplace_back(plane);

        const auto nodeIndex = state.m_nodes.size();
        state.m_nodes.emplace_back();

        cNode_t node{};
        node.children[0] = LoadTreeNode(memory, clipMap, state, *tree.m_node->m_front);
        node.children[1] = LoadTreeNode(memory, clipMap, state, *tree.m_node->m_back);
        state.m_nodes[nodeIndex] = node;

        return static_cast<int16_t>(nodeIndex);
    }

    bool LoadPartitions(MemoryManager& memory, clipMap_t& clipMap, const map::T6MapWorldGeometry& collisionWorld)
    {
        if (collisionWorld.m_vertices.size() > MAX_COLLISION_VERTS)
        {
            con::error("T6 custom map collision vertex count {} exceeds the uint16 limit.", collisionWorld.m_vertices.size());
            return false;
        }

        clipMap.vertCount = static_cast<unsigned int>(collisionWorld.m_vertices.size());
        clipMap.verts = memory.Alloc<vec3_t>(clipMap.vertCount);
        for (auto vertexIndex = 0u; vertexIndex < clipMap.vertCount; vertexIndex++)
            clipMap.verts[vertexIndex] = collisionWorld.m_vertices[vertexIndex].m_position;

        std::vector<std::uint16_t> triIndices;
        for (const auto& surface : collisionWorld.m_surfaces)
        {
            for (auto indexOffset = 0u; indexOffset < surface.m_tri_count * 3u; indexOffset++)
            {
                const auto triIndex = static_cast<std::uint16_t>(collisionWorld.m_indices[surface.m_first_index + indexOffset] + surface.m_first_vertex);
                triIndices.emplace_back(triIndex);
            }
        }

        clipMap.triCount = static_cast<int>(triIndices.size() / 3u);
        clipMap.triIndices = reinterpret_cast<std::uint16_t (*)[3]>(memory.Alloc<std::uint16_t>(triIndices.size()));
        std::memcpy(clipMap.triIndices, triIndices.data(), sizeof(std::uint16_t) * triIndices.size());

        std::vector<CollisionPartition> partitions;
        std::vector<std::uint16_t> uniqueIndices;
        for (const auto& surface : collisionWorld.m_surfaces)
        {
            const auto firstTri = surface.m_first_index / 3u;
            for (auto triIndex = 0u; triIndex < surface.m_tri_count; triIndex++)
            {
                CollisionPartition partition{};
                partition.triCount = 1;
                partition.firstTri = static_cast<int>(firstTri + triIndex);
                partition.nuinds = 3;
                partition.fuind = static_cast<int>(uniqueIndices.size());

                const auto* tri = clipMap.triIndices[partition.firstTri];
                uniqueIndices.emplace_back(tri[0]);
                uniqueIndices.emplace_back(tri[1]);
                uniqueIndices.emplace_back(tri[2]);

                partitions.emplace_back(partition);
            }
        }

        clipMap.partitionCount = static_cast<int>(partitions.size());
        clipMap.partitions = memory.Alloc<CollisionPartition>(partitions.size());
        std::memcpy(clipMap.partitions, partitions.data(), sizeof(CollisionPartition) * partitions.size());

        clipMap.info.nuinds = static_cast<unsigned int>(uniqueIndices.size());
        clipMap.info.uinds = memory.Alloc<std::uint16_t>(uniqueIndices.size());
        std::memcpy(clipMap.info.uinds, uniqueIndices.data(), sizeof(std::uint16_t) * uniqueIndices.size());

        return true;
    }

    void LoadEmptyLeafBrushTree(MemoryManager& memory, clipMap_t& clipMap)
    {
        clipMap.info.numBrushSides = 0u;
        clipMap.info.brushsides = nullptr;

        clipMap.info.numLeafBrushes = 1u;
        clipMap.info.leafbrushes = AllocZeroed<LeafBrush>(memory, clipMap.info.numLeafBrushes);

        clipMap.info.leafbrushNodesCount = 2u;
        clipMap.info.leafbrushNodes = AllocZeroed<cLeafBrushNode_s>(memory, clipMap.info.leafbrushNodesCount);
        auto& leafBrushNode = clipMap.info.leafbrushNodes[GENERATED_LEAF_BRUSH_NODE];
        leafBrushNode.axis = TERMINAL_LEAF_BRUSH_AXIS;
        leafBrushNode.leafBrushCount = 1;
        leafBrushNode.contents = GENERATED_LEAF_BRUSH_CONTENTS;
        leafBrushNode.data.leaf.brushes = clipMap.info.leafbrushes;

        clipMap.info.numBrushVerts = 0u;
        clipMap.info.brushVerts = nullptr;

        clipMap.info.numBrushes = 1u;
        clipMap.info.brushes = AllocZeroed<cbrush_array_t>(memory, clipMap.info.numBrushes);
        clipMap.info.brushes[0].contents = GENERATED_LEAF_BRUSH_CONTENTS;

        clipMap.info.brushBounds = AllocZeroed<BoundsArray>(memory, clipMap.info.numBrushes);
        clipMap.info.brushContents = AllocZeroed<int>(memory, clipMap.info.numBrushes);
        clipMap.info.brushContents[0] = GENERATED_LEAF_BRUSH_CONTENTS;
    }

    void LoadBspTree(MemoryManager& memory, clipMap_t& clipMap)
    {
        vec3_t worldMins = clipMap.verts[0];
        vec3_t worldMaxs = clipMap.verts[0];
        for (auto vertexIndex = 1u; vertexIndex < clipMap.vertCount; vertexIndex++)
            UpdateAabbWithPoint(clipMap.verts[vertexIndex], worldMins, worldMaxs);

        const auto tree = std::make_unique<Tree>(worldMins, worldMaxs, 0);

        for (auto partitionIndex = 0; partitionIndex < clipMap.partitionCount; partitionIndex++)
        {
            const auto& partition = clipMap.partitions[partitionIndex];
            auto partitionMins = clipMap.verts[clipMap.info.uinds[partition.fuind]];
            auto partitionMaxs = partitionMins;
            for (auto uindIndex = 1; uindIndex < partition.nuinds; uindIndex++)
                UpdateAabbWithPoint(clipMap.verts[clipMap.info.uinds[partition.fuind + uindIndex]], partitionMins, partitionMaxs);

            tree->AddObject(std::make_shared<TreeObject>(TreeObject{partitionMins, partitionMaxs, partitionIndex}));
        }

        ClipBuildState state;
        state.m_leafs.emplace_back();
        LoadTreeNode(memory, clipMap, state, *tree);

        clipMap.info.planeCount = static_cast<int>(state.m_planes.size());
        clipMap.info.planes = memory.Alloc<cplane_s>(state.m_planes.size());
        std::memcpy(clipMap.info.planes, state.m_planes.data(), sizeof(cplane_s) * state.m_planes.size());

        clipMap.numNodes = static_cast<unsigned int>(state.m_nodes.size());
        clipMap.nodes = memory.Alloc<cNode_t>(state.m_nodes.size());
        std::memcpy(clipMap.nodes, state.m_nodes.data(), sizeof(cNode_t) * state.m_nodes.size());

        clipMap.numLeafs = static_cast<unsigned int>(state.m_leafs.size());
        clipMap.leafs = memory.Alloc<cLeaf_s>(state.m_leafs.size());
        std::memcpy(clipMap.leafs, state.m_leafs.data(), sizeof(cLeaf_s) * state.m_leafs.size());

        clipMap.aabbTreeCount = static_cast<int>(state.m_aabb_trees.size());
        clipMap.aabbTrees = memory.Alloc<CollisionAabbTree>(state.m_aabb_trees.size());
        std::memcpy(clipMap.aabbTrees, state.m_aabb_trees.data(), sizeof(CollisionAabbTree) * state.m_aabb_trees.size());

        for (auto nodeIndex = 0u; nodeIndex < clipMap.numNodes; nodeIndex++)
            clipMap.nodes[nodeIndex].plane = &clipMap.info.planes[nodeIndex];
    }

    bool LoadWorldCollision(MemoryManager& memory, clipMap_t& clipMap, const map::T6MapWorldGeometry& collisionWorld)
    {
        LoadEmptyLeafBrushTree(memory, clipMap);

        if (!LoadPartitions(memory, clipMap, collisionWorld))
            return false;

        LoadBspTree(memory, clipMap);

        const auto walkableEdgeSize = (3 * clipMap.triCount + 31) / 32 * 4;
        clipMap.triEdgeIsWalkable = memory.Alloc<char>(walkableEdgeSize);
        std::memset(clipMap.triEdgeIsWalkable, 1, walkableEdgeSize);

        return true;
    }
} // namespace

namespace map
{
    clipMap_t* CreateClipMapT6(MemoryManager& memory, AssetCreationContext& context, const T6MapGeometry& geometry, const GfxWorld& gfxWorld)
    {
        auto* clipMap = AllocZeroed<clipMap_t>(memory);
        clipMap->name = memory.Dup(geometry.m_world_asset_name.c_str());
        clipMap->isInUse = 1;
        clipMap->checksum = 0u;
        clipMap->pInfo = nullptr;

        auto* mapEntsAsset = context.LoadDependency<AssetMapEnts>(geometry.m_world_asset_name);
        if (!mapEntsAsset)
            return nullptr;

        clipMap->mapEnts = mapEntsAsset->Asset();

        LoadBoxData(*clipMap);

        LoadVisibility(memory, *clipMap);

        LoadRopesAndConstraints(memory, *clipMap);

        LoadSubModelCollision(memory, *clipMap, gfxWorld);

        LoadDynEnts(memory, *clipMap);

        LoadXModelCollision(*clipMap);

        clipMap->info.numMaterials = 1u;
        clipMap->info.materials = AllocZeroed<ClipMaterial>(memory, 1u);
        clipMap->info.materials[0].name = memory.Dup(DEFAULT_CLIP_MATERIAL);
        clipMap->info.materials[0].contentFlags = MATERIAL_CONTENT_FLAGS;
        clipMap->info.materials[0].surfaceFlags = MATERIAL_SURFACE_FLAGS;

        if (!LoadWorldCollision(memory, *clipMap, geometry.m_collision_world))
            return nullptr;

        FinalizeBoxModelLeafBrushNode(*clipMap);
        FinalizeBoxBrush(memory, *clipMap);

        con::debug(
            "T6 custom map clipmap \"{}\": verts={} tris={} partitions={} aabbTrees={} nodes={} leafs={} subModels={} clusters={} clusterBytes={} dynEntCount=({}, {}, {}, {})",
            geometry.m_world_asset_name,
            clipMap->vertCount,
            clipMap->triCount,
            clipMap->partitionCount,
            clipMap->aabbTreeCount,
            clipMap->numNodes,
            clipMap->numLeafs,
            clipMap->numSubModels,
            clipMap->numClusters,
            clipMap->clusterBytes,
            clipMap->dynEntCount[0],
            clipMap->dynEntCount[1],
            clipMap->dynEntCount[2],
            clipMap->dynEntCount[3]);

        return clipMap;
    }
} // namespace map
