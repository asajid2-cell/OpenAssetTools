#include "MapWorldT6.h"

#include "Game/T6/T6.h"
#include "Utils/Logging/Log.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <format>
#include <limits>

using namespace T6;

namespace
{
    constexpr auto T6_GFX_LIGHT_TYPE_DIR = 1;
    constexpr auto T6_RESERVED_PATH_NODE_COUNT = 128u;
    constexpr auto DEG_TO_RAD = 0.01745329251994329576923690768489f;
    constexpr auto DEFAULT_SUN_DIRECTION_X = -0.242f;
    constexpr auto DEFAULT_SUN_DIRECTION_Y = -0.841f;
    constexpr auto DEFAULT_SUN_DIRECTION_Z = -0.485f;

    template<typename T> T* AllocZeroed(MemoryManager& memory, const std::size_t count = 1u)
    {
        auto* result = memory.Alloc<T>(count);
        std::memset(result, 0, sizeof(T) * count);
        return result;
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
        path.visBytes = 0;
        path.pathVis = nullptr;
        path.smoothBytes = 0;
        path.smoothCache = nullptr;
        path.nodeTreeCount = 1;
        path.nodeTree = AllocZeroed<pathnode_tree_t>(memory);
        path.nodeTree[0].axis = -1;
        path.nodeTree[0].dist = 0.0f;
        path.nodeTree[0].u.s.nodeCount = static_cast<int>(path.nodeCount);
        path.nodeTree[0].u.s.nodes = AllocZeroed<uint16_t>(memory, path.nodeCount);

        for (auto nodeIndex = 0u; nodeIndex < path.nodeCount; nodeIndex++)
        {
            const auto& sourceNode = entitySource.m_path_nodes[nodeIndex];
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
            node->constant.totalLinkCount = 0u;
            node->constant.Links = nullptr;

            auto* baseNode = &path.basenodes[nodeIndex];
            baseNode->vOrigin = node->constant.vOrigin;
            baseNode->type = NODE_PATHNODE;

            path.nodeTree[0].u.s.nodes[nodeIndex] = static_cast<uint16_t>(nodeIndex);
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
