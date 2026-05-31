#include "MapWorldT6.h"

#include "Game/T6/T6.h"
#include "Utils/Logging/Log.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <vector>

using namespace T6;

namespace
{
    constexpr auto T6_GFX_LIGHT_TYPE_DIR = 1;
    constexpr auto T6_RESERVED_PATH_NODE_COUNT = 128u;
    constexpr auto DEG_TO_RAD = 0.01745329251994329576923690768489f;
    constexpr auto DEFAULT_SUN_DIRECTION_X = -0.242f;
    constexpr auto DEFAULT_SUN_DIRECTION_Y = -0.841f;
    constexpr auto DEFAULT_SUN_DIRECTION_Z = -0.485f;
    constexpr auto GENERATED_PATH_LINK_DISTANCE = 512.0f;
    constexpr auto GENERATED_PATH_LINK_MAX_VERTICAL_DELTA = 128.0f;
    constexpr auto GENERATED_PATH_LINK_FLAGS = 0x28;
    constexpr auto GENERATED_PATH_TREE_LEAF_NODE_COUNT = 2u;

    struct GeneratedPathLink
    {
        uint16_t m_node_num;
        float m_distance;
    };

    struct GeneratedPathTreeNode
    {
        int m_axis = -1;
        float m_dist = 0.0f;
        int m_child_indices[2] = {-1, -1};
        std::vector<uint16_t> m_nodes;
    };

    template<typename T> T* AllocZeroed(MemoryManager& memory, const std::size_t count = 1u)
    {
        auto* result = memory.Alloc<T>(count);
        std::memset(result, 0, sizeof(T) * count);
        return result;
    }

    [[nodiscard]] float Distance3d(const std::array<float, 3>& lhs, const std::array<float, 3>& rhs)
    {
        const auto dx = lhs[0] - rhs[0];
        const auto dy = lhs[1] - rhs[1];
        const auto dz = lhs[2] - rhs[2];
        return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    }

    [[nodiscard]] bool ShouldGeneratePathLink(const map::T6MapPathNode& lhs, const map::T6MapPathNode& rhs, float& distance)
    {
        if (std::fabs(lhs.m_origin[2] - rhs.m_origin[2]) > GENERATED_PATH_LINK_MAX_VERTICAL_DELTA)
            return false;

        distance = Distance3d(lhs.m_origin, rhs.m_origin);
        return distance <= GENERATED_PATH_LINK_DISTANCE;
    }

    [[nodiscard]] std::vector<std::vector<GeneratedPathLink>> BuildGeneratedPathLinks(const std::vector<map::T6MapPathNode>& pathNodes)
    {
        std::vector<std::vector<GeneratedPathLink>> result(pathNodes.size());

        for (auto nodeIndex = 0u; nodeIndex < pathNodes.size(); nodeIndex++)
        {
            for (auto otherNodeIndex = nodeIndex + 1u; otherNodeIndex < pathNodes.size(); otherNodeIndex++)
            {
                float distance;
                if (!ShouldGeneratePathLink(pathNodes[nodeIndex], pathNodes[otherNodeIndex], distance))
                    continue;

                result[nodeIndex].push_back({static_cast<uint16_t>(otherNodeIndex), distance});
                result[otherNodeIndex].push_back({static_cast<uint16_t>(nodeIndex), distance});
            }
        }

        return result;
    }

    [[nodiscard]] int SelectPathTreeSplitAxis(const std::vector<map::T6MapPathNode>& pathNodes, const std::vector<uint16_t>& nodeIndices)
    {
        auto bestAxis = 0;
        auto bestRange = -1.0f;

        for (auto axis = 0; axis < 3; axis++)
        {
            auto minValue = pathNodes[nodeIndices[0]].m_origin[axis];
            auto maxValue = minValue;
            for (const auto nodeIndex : nodeIndices)
            {
                minValue = std::min(minValue, pathNodes[nodeIndex].m_origin[axis]);
                maxValue = std::max(maxValue, pathNodes[nodeIndex].m_origin[axis]);
            }

            const auto range = maxValue - minValue;
            if (range > bestRange)
            {
                bestAxis = axis;
                bestRange = range;
            }
        }

        return bestAxis;
    }

    int BuildGeneratedPathTreeNode(std::vector<GeneratedPathTreeNode>& treeNodes,
                                   const std::vector<map::T6MapPathNode>& pathNodes,
                                   std::vector<uint16_t> nodeIndices)
    {
        const auto treeIndex = static_cast<int>(treeNodes.size());
        treeNodes.emplace_back();

        if (nodeIndices.size() <= GENERATED_PATH_TREE_LEAF_NODE_COUNT)
        {
            treeNodes[treeIndex].m_nodes = std::move(nodeIndices);
            return treeIndex;
        }

        const auto axis = SelectPathTreeSplitAxis(pathNodes, nodeIndices);
        std::sort(nodeIndices.begin(),
                  nodeIndices.end(),
                  [&pathNodes, axis](const uint16_t lhs, const uint16_t rhs)
                  {
                      return pathNodes[lhs].m_origin[axis] < pathNodes[rhs].m_origin[axis];
                  });

        const auto splitIndex = nodeIndices.size() / 2u;
        std::vector<uint16_t> left(nodeIndices.begin(), nodeIndices.begin() + splitIndex);
        std::vector<uint16_t> right(nodeIndices.begin() + splitIndex, nodeIndices.end());

        treeNodes[treeIndex].m_axis = axis;
        treeNodes[treeIndex].m_dist = (pathNodes[left.back()].m_origin[axis] + pathNodes[right.front()].m_origin[axis]) * 0.5f;
        treeNodes[treeIndex].m_child_indices[0] = BuildGeneratedPathTreeNode(treeNodes, pathNodes, std::move(left));
        treeNodes[treeIndex].m_child_indices[1] = BuildGeneratedPathTreeNode(treeNodes, pathNodes, std::move(right));
        return treeIndex;
    }

    void InitPathNodeTree(MemoryManager& memory, PathData& path, const std::vector<map::T6MapPathNode>& pathNodes)
    {
        assert(!pathNodes.empty());
        assert(path.nodeCount == pathNodes.size());

        std::vector<uint16_t> nodeIndices;
        nodeIndices.reserve(pathNodes.size());
        for (auto nodeIndex = 0u; nodeIndex < pathNodes.size(); nodeIndex++)
            nodeIndices.emplace_back(static_cast<uint16_t>(nodeIndex));

        std::vector<GeneratedPathTreeNode> generatedTree;
        BuildGeneratedPathTreeNode(generatedTree, pathNodes, std::move(nodeIndices));

        path.nodeTreeCount = static_cast<int>(generatedTree.size());
        path.nodeTree = AllocZeroed<pathnode_tree_t>(memory, generatedTree.size());
        for (auto treeIndex = 0u; treeIndex < generatedTree.size(); treeIndex++)
        {
            const auto& sourceNode = generatedTree[treeIndex];
            auto& runtimeNode = path.nodeTree[treeIndex];
            runtimeNode.axis = sourceNode.m_axis;
            runtimeNode.dist = sourceNode.m_dist;

            if (runtimeNode.axis < 0)
            {
                runtimeNode.u.s.nodeCount = static_cast<int>(sourceNode.m_nodes.size());
                runtimeNode.u.s.nodes = AllocZeroed<uint16_t>(memory, sourceNode.m_nodes.size());
                std::memcpy(runtimeNode.u.s.nodes, sourceNode.m_nodes.data(), sizeof(uint16_t) * sourceNode.m_nodes.size());
            }
            else
            {
                runtimeNode.u.child[0] = &path.nodeTree[sourceNode.m_child_indices[0]];
                runtimeNode.u.child[1] = &path.nodeTree[sourceNode.m_child_indices[1]];
            }
        }
    }

    [[nodiscard]] bool InitPathVis(MemoryManager& memory, PathData& path)
    {
        if (path.nodeCount < 2u)
        {
            path.visBytes = 0;
            path.pathVis = nullptr;
            return true;
        }

        const auto visibilityBitCount = static_cast<uint64_t>(path.nodeCount) * static_cast<uint64_t>(path.nodeCount - 1u);
        const auto visibilityBytes = static_cast<uint64_t>((visibilityBitCount + 7u) / 8u);
        if (visibilityBytes > static_cast<uint64_t>(std::numeric_limits<int>::max()))
        {
            con::error("T6 custom map has too many authored path nodes for generated path visibility: {}", path.nodeCount);
            return false;
        }

        path.visBytes = static_cast<int>(visibilityBytes);
        path.pathVis = AllocZeroed<char>(memory, static_cast<std::size_t>(path.visBytes));
        std::memset(path.pathVis, 0xFF, static_cast<std::size_t>(path.visBytes));
        return true;
    }

    void InitEmptyPathData(MemoryManager& memory, PathData& path)
    {
        path.nodeCount = 0u;
        path.originalNodeCount = 0u;
        path.visBytes = 0;
        path.smoothBytes = 0;
        path.nodeTreeCount = 0;

        path.nodes = AllocZeroed<pathnode_t>(memory, T6_RESERVED_PATH_NODE_COUNT);
        path.basenodes = AllocZeroed<pathbasenode_t>(memory, T6_RESERVED_PATH_NODE_COUNT);
        path.pathVis = nullptr;
        path.smoothCache = nullptr;
        path.nodeTree = nullptr;
    }

    bool InitPathData(MemoryManager& memory, PathData& path, const map::T6MapEntitySource& entitySource)
    {
        if (entitySource.m_path_nodes.empty())
        {
            InitEmptyPathData(memory, path);
            return true;
        }

        if (entitySource.m_path_nodes.size() > std::numeric_limits<uint16_t>::max())
        {
            con::error("T6 custom map has too many authored path nodes: {}", entitySource.m_path_nodes.size());
            return false;
        }

        path.nodeCount = static_cast<unsigned int>(entitySource.m_path_nodes.size());
        path.originalNodeCount = path.nodeCount;
        path.nodes = AllocZeroed<pathnode_t>(memory, path.nodeCount + T6_RESERVED_PATH_NODE_COUNT);
        path.basenodes = AllocZeroed<pathbasenode_t>(memory, path.nodeCount + T6_RESERVED_PATH_NODE_COUNT);
        if (!InitPathVis(memory, path))
            return false;
        path.smoothBytes = 0;
        path.smoothCache = nullptr;
        InitPathNodeTree(memory, path, entitySource.m_path_nodes);

        const auto generatedLinks = BuildGeneratedPathLinks(entitySource.m_path_nodes);

        for (auto nodeIndex = 0u; nodeIndex < path.nodeCount; nodeIndex++)
        {
            const auto& sourceNode = entitySource.m_path_nodes[nodeIndex];
            const auto& sourceLinks = generatedLinks[nodeIndex];
            if (sourceLinks.size() > static_cast<std::size_t>(std::numeric_limits<int16_t>::max()))
            {
                con::error("T6 custom map path node {} generated too many path links: {}", nodeIndex, sourceLinks.size());
                return false;
            }

            auto* node = &path.nodes[nodeIndex];

            node->constant.type = NODE_PATHNODE;
            node->constant.spawnflags = sourceNode.m_spawn_flags;
            node->constant.vOrigin.x = sourceNode.m_origin[0];
            node->constant.vOrigin.y = sourceNode.m_origin[1];
            node->constant.vOrigin.z = sourceNode.m_origin[2];
            node->constant.fAngle = sourceNode.m_yaw;

            const auto yawRadians = sourceNode.m_yaw * DEG_TO_RAD;
            node->constant.forward.x = std::cos(yawRadians);
            node->constant.forward.y = std::sin(yawRadians);
            node->constant.fRadius = 0.0f;
            node->constant.minUseDistSq = 0.0f;
            node->constant.wOverlapNode[0] = -1;
            node->constant.wOverlapNode[1] = -1;
            node->constant.totalLinkCount = static_cast<uint16_t>(sourceLinks.size());
            node->constant.Links = sourceLinks.empty() ? nullptr : AllocZeroed<pathlink_s>(memory, sourceLinks.size());

            for (auto linkIndex = 0u; linkIndex < sourceLinks.size(); linkIndex++)
            {
                node->constant.Links[linkIndex].fDist = sourceLinks[linkIndex].m_distance;
                node->constant.Links[linkIndex].nodeNum = sourceLinks[linkIndex].m_node_num;
                node->constant.Links[linkIndex].flags = GENERATED_PATH_LINK_FLAGS;
            }

            node->dynamic.wLinkCount = 0;

            auto* baseNode = &path.basenodes[nodeIndex];
            baseNode->vOrigin = node->constant.vOrigin;
            baseNode->type = NODE_PATHNODE;
        }

        return true;
    }

    MapEnts* CreateMapEnts(MemoryManager& memory, const std::string& assetName, const map::T6MapEntitySource& entitySource)
    {
        auto* mapEnts = AllocZeroed<MapEnts>(memory);
        mapEnts->name = memory.Dup(assetName.c_str());
        mapEnts->entityString = memory.Dup(entitySource.m_entity_string.c_str());
        mapEnts->numEntityChars = static_cast<int>(entitySource.m_entity_string.length() + 1u);

        mapEnts->trigger.count = 0u;
        mapEnts->trigger.models = nullptr;
        mapEnts->trigger.hullCount = 0u;
        mapEnts->trigger.hulls = nullptr;
        mapEnts->trigger.slabCount = 0u;
        mapEnts->trigger.slabs = nullptr;

        return mapEnts;
    }

    ComWorld* CreateComWorld(MemoryManager& memory, const std::string& assetName)
    {
        auto* comWorld = AllocZeroed<ComWorld>(memory);
        comWorld->name = memory.Dup(assetName.c_str());
        comWorld->isInUse = 1;

        comWorld->primaryLightCount = 2u;
        comWorld->primaryLights = AllocZeroed<ComPrimaryLight>(memory, comWorld->primaryLightCount);

        auto* sunLight = &comWorld->primaryLights[1];
        sunLight->type = T6_GFX_LIGHT_TYPE_DIR;
        sunLight->canUseShadowMap = 0;
        sunLight->color.x = 1.0f;
        sunLight->color.y = 0.89f;
        sunLight->color.z = 0.69f;
        sunLight->dir.x = DEFAULT_SUN_DIRECTION_X;
        sunLight->dir.y = DEFAULT_SUN_DIRECTION_Y;
        sunLight->dir.z = DEFAULT_SUN_DIRECTION_Z;
        sunLight->diffuseColor.x = 1.0f;
        sunLight->diffuseColor.y = 0.89f;
        sunLight->diffuseColor.z = 0.69f;
        sunLight->diffuseColor.w = 13.5f;

        return comWorld;
    }

    GameWorldSp* CreateGameWorldSp(MemoryManager& memory, const std::string& assetName, const map::T6MapEntitySource& entitySource)
    {
        auto* gameWorld = AllocZeroed<GameWorldSp>(memory);
        gameWorld->name = memory.Dup(assetName.c_str());
        if (!InitPathData(memory, gameWorld->path, entitySource))
            return nullptr;

        return gameWorld;
    }

    GameWorldMp* CreateGameWorldMp(MemoryManager& memory, const std::string& assetName, const map::T6MapEntitySource& entitySource)
    {
        auto* gameWorld = AllocZeroed<GameWorldMp>(memory);
        gameWorld->name = memory.Dup(assetName.c_str());
        if (!InitPathData(memory, gameWorld->path, entitySource))
            return nullptr;

        return gameWorld;
    }

    SkinnedVertsDef* CreateSkinnedVerts(MemoryManager& memory)
    {
        auto* skinnedVerts = AllocZeroed<SkinnedVertsDef>(memory);
        skinnedVerts->name = memory.Dup("skinnedverts");
        skinnedVerts->maxSkinnedVerts = 0u;

        return skinnedVerts;
    }
} // namespace

namespace map
{
    std::string GetT6MapWorldAssetName(const std::string& mapName)
    {
        return std::format("maps/mp/{}.d3dbsp", mapName);
    }

    bool EmitBaseWorldAssetsT6(AssetCreationContext& context,
                               Zone& zone,
                               const std::string& mapAssetName,
                               const T6MapEntitySource& entitySource,
                               const ZoneDefinitionMapType mapType)
    {
        if (mapType == ZoneDefinitionMapType::NONE)
        {
            con::error("Cannot emit T6 base world assets for a non-map zone");
            return false;
        }

        auto& memory = zone.Memory();
        const auto assetName = GetT6MapWorldAssetName(mapAssetName);

        if (!EmitPreGfxWorldAssetsT6(context, zone, mapAssetName))
            return false;

        return EmitRuntimeWorldAssetsT6(context, zone, mapAssetName, entitySource, mapType);
    }

    bool EmitPreGfxWorldAssetsT6(AssetCreationContext& context, Zone& zone, const std::string& mapAssetName)
    {
        auto& memory = zone.Memory();
        const auto assetName = GetT6MapWorldAssetName(mapAssetName);

        if (!context.HasAsset<AssetSkinnedVerts>("skinnedverts"))
            context.AddAsset<AssetSkinnedVerts>("skinnedverts", CreateSkinnedVerts(memory));

        if (!context.HasAsset<AssetComWorld>(assetName))
        {
            auto* comWorld = CreateComWorld(memory, assetName);
            if (!comWorld)
                return false;

            context.AddAsset<AssetComWorld>(assetName, comWorld);
        }

        return true;
    }

    bool EmitRuntimeWorldAssetsT6(AssetCreationContext& context,
                                  Zone& zone,
                                  const std::string& mapAssetName,
                                  const T6MapEntitySource& entitySource,
                                  const ZoneDefinitionMapType mapType)
    {
        if (mapType == ZoneDefinitionMapType::NONE)
        {
            con::error("Cannot emit T6 world assets for a non-map zone");
            return false;
        }

        auto& memory = zone.Memory();
        const auto assetName = GetT6MapWorldAssetName(mapAssetName);

        if (mapType == ZoneDefinitionMapType::SP)
        {
            if (!context.HasAsset<AssetGameWorldSp>(assetName))
            {
                auto* gameWorldSp = CreateGameWorldSp(memory, assetName, entitySource);
                if (!gameWorldSp)
                    return false;

                context.AddAsset<AssetGameWorldSp>(assetName, gameWorldSp);
            }
        }
        else if (!context.HasAsset<AssetGameWorldMp>(assetName))
        {
            auto* gameWorldMp = CreateGameWorldMp(memory, assetName, entitySource);
            if (!gameWorldMp)
                return false;

            context.AddAsset<AssetGameWorldMp>(assetName, gameWorldMp);
        }

        if (!context.HasAsset<AssetMapEnts>(assetName))
        {
            auto* mapEnts = CreateMapEnts(memory, assetName, entitySource);
            if (!mapEnts)
                return false;

            context.AddAsset<AssetMapEnts>(assetName, mapEnts);
        }

        return true;
    }
} // namespace map
