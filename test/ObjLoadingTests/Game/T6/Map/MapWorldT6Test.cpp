#include "Asset/AssetCreationContext.h"
#include "Asset/AssetCreatorCollection.h"
#include "Game/T6/Map/MapWorldT6.h"
#include "Game/T6/T6.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <string>
#include <vector>

using namespace T6;
using namespace std::literals;

namespace
{
    TEST_CASE("T6 map world asset name uses conventional maps/mp d3dbsp path", "[t6][map]")
    {
        REQUIRE(map::GetT6MapWorldAssetName("zm_example") == "maps/mp/zm_example.d3dbsp");
    }

    TEST_CASE("T6 zombie map emits base MP world asset", "[t6][map]")
    {
        Zone zone("zm_example", 0, GameId::T6, GamePlatform::PC);
        AssetCreatorCollection creatorCollection(zone);
        IgnoredAssetLookup ignoredAssetLookup;
        AssetCreationContext context(zone, &creatorCollection, &ignoredAssetLookup);

        map::T6MapEntitySource entitySource;
        entitySource.m_entity_string = "{\n\"classname\" \"worldspawn\"\n}\n";
        entitySource.m_path_nodes.push_back({{128.0f, 256.0f, 64.0f}, 90.0f, 4});

        REQUIRE(map::EmitBaseWorldAssetsT6(context, zone, zone.m_name, entitySource, ZoneDefinitionMapType::ZM));

        const auto assetName = map::GetT6MapWorldAssetName(zone.m_name);
        const auto* comWorldInfo = zone.m_pools.GetAsset<AssetComWorld>(assetName);
        const auto* mapEntsInfo = zone.m_pools.GetAsset<AssetMapEnts>(assetName);
        const auto* gameWorldMpInfo = zone.m_pools.GetAsset<AssetGameWorldMp>(assetName);
        const auto* gameWorldSpInfo = zone.m_pools.GetAsset<AssetGameWorldSp>(assetName);
        const auto* skinnedVertsInfo = zone.m_pools.GetAsset<AssetSkinnedVerts>("skinnedverts");

        REQUIRE(comWorldInfo != nullptr);
        REQUIRE(mapEntsInfo != nullptr);
        REQUIRE(gameWorldMpInfo != nullptr);
        REQUIRE(gameWorldSpInfo == nullptr);
        REQUIRE(skinnedVertsInfo != nullptr);

        const auto* mapEnts = mapEntsInfo->Asset();
        REQUIRE(mapEnts->name == assetName);
        REQUIRE(mapEnts->entityString == entitySource.m_entity_string);
        REQUIRE(mapEnts->numEntityChars == static_cast<int>(entitySource.m_entity_string.length() + 1u));
        REQUIRE(mapEnts->trigger.count == 0u);

        const auto* comWorld = comWorldInfo->Asset();
        REQUIRE(comWorld->isInUse == 1);
        REQUIRE(comWorld->primaryLightCount == 2u);
        REQUIRE(comWorld->primaryLights != nullptr);
        REQUIRE(comWorld->primaryLights[1].type == 1);
        REQUIRE(comWorld->primaryLights[1].dir.x == -0.242f);
        REQUIRE(comWorld->primaryLights[1].dir.y == -0.841f);
        REQUIRE(comWorld->primaryLights[1].dir.z == -0.485f);

        const auto* gameWorldMp = gameWorldMpInfo->Asset();
        REQUIRE(gameWorldMp->path.nodeCount == 1u);
        REQUIRE(gameWorldMp->path.originalNodeCount == 1u);
        REQUIRE(gameWorldMp->path.visBytes == 0);
        REQUIRE(gameWorldMp->path.smoothBytes == 0);
        REQUIRE(gameWorldMp->path.nodeTreeCount == 1);
        REQUIRE(gameWorldMp->path.nodes != nullptr);
        REQUIRE(gameWorldMp->path.basenodes != nullptr);
        REQUIRE(gameWorldMp->path.pathVis == nullptr);
        REQUIRE(gameWorldMp->path.smoothCache == nullptr);
        REQUIRE(gameWorldMp->path.nodeTree != nullptr);
        REQUIRE(gameWorldMp->path.nodes[0].constant.type == NODE_PATHNODE);
        REQUIRE(gameWorldMp->path.nodes[0].constant.spawnflags == 4);
        REQUIRE(gameWorldMp->path.nodes[0].constant.vOrigin.x == 128.0f);
        REQUIRE(gameWorldMp->path.nodes[0].constant.vOrigin.y == 256.0f);
        REQUIRE(gameWorldMp->path.nodes[0].constant.vOrigin.z == 64.0f);
        REQUIRE(gameWorldMp->path.nodes[0].constant.fAngle == 90.0f);
        REQUIRE(gameWorldMp->path.nodes[0].constant.forward.x == Catch::Approx(0.0f).margin(0.0001f));
        REQUIRE(gameWorldMp->path.nodes[0].constant.forward.y == Catch::Approx(1.0f).margin(0.0001f));
        REQUIRE(gameWorldMp->path.nodes[0].constant.totalLinkCount == 0u);
        REQUIRE(gameWorldMp->path.nodes[0].constant.Links == nullptr);
        REQUIRE(gameWorldMp->path.basenodes[0].type == NODE_PATHNODE);
        REQUIRE(gameWorldMp->path.nodeTree[0].axis == -1);
        REQUIRE(gameWorldMp->path.nodeTree[0].u.s.nodeCount == 1);
        REQUIRE(gameWorldMp->path.nodeTree[0].u.s.nodes != nullptr);
        REQUIRE(gameWorldMp->path.nodeTree[0].u.s.nodes[0] == 0u);

        REQUIRE(skinnedVertsInfo->Asset()->maxSkinnedVerts == 0u);
    }

    TEST_CASE("T6 multiplayer map emits base MP world asset", "[t6][map]")
    {
        Zone zone("mp_example", 0, GameId::T6, GamePlatform::PC);
        AssetCreatorCollection creatorCollection(zone);
        IgnoredAssetLookup ignoredAssetLookup;
        AssetCreationContext context(zone, &creatorCollection, &ignoredAssetLookup);

        map::T6MapEntitySource entitySource;
        entitySource.m_entity_string = "{\n\"classname\" \"worldspawn\"\n}\n";

        REQUIRE(map::EmitBaseWorldAssetsT6(context, zone, zone.m_name, entitySource, ZoneDefinitionMapType::MP));

        const auto assetName = map::GetT6MapWorldAssetName(zone.m_name);
        const auto* gameWorldMpInfo = zone.m_pools.GetAsset<AssetGameWorldMp>(assetName);
        REQUIRE(gameWorldMpInfo != nullptr);
        REQUIRE(zone.m_pools.GetAsset<AssetGameWorldSp>(assetName) == nullptr);
        REQUIRE(gameWorldMpInfo->Asset()->path.nodeCount == 0u);
        REQUIRE(gameWorldMpInfo->Asset()->path.nodeTreeCount == 0);
    }

    TEST_CASE("T6 map structural assets follow stock runtime order", "[t6][map]")
    {
        Zone zone("zm_example", 0, GameId::T6, GamePlatform::PC);
        AssetCreatorCollection creatorCollection(zone);
        IgnoredAssetLookup ignoredAssetLookup;
        AssetCreationContext context(zone, &creatorCollection, &ignoredAssetLookup);

        const auto assetName = map::GetT6MapWorldAssetName(zone.m_name);
        map::T6MapEntitySource entitySource;
        entitySource.m_entity_string = "{\n\"classname\" \"worldspawn\"\n}\n";

        REQUIRE(map::EmitPreGfxWorldAssetsT6(context, zone, zone.m_name));

        auto* gfxWorld = zone.Memory().Alloc<GfxWorld>();
        gfxWorld->name = zone.Memory().Dup(assetName.c_str());
        context.AddAsset<AssetGfxWorld>(assetName, gfxWorld);

        REQUIRE(map::EmitRuntimeWorldAssetsT6(context, zone, zone.m_name, entitySource, ZoneDefinitionMapType::ZM));

        std::vector<asset_type_t> assetTypes;
        for (const auto& asset : zone.m_pools)
            assetTypes.emplace_back(asset->m_type);

        REQUIRE(assetTypes.size() == 5u);
        REQUIRE(assetTypes[0] == AssetSkinnedVerts::EnumEntry);
        REQUIRE(assetTypes[1] == AssetComWorld::EnumEntry);
        REQUIRE(assetTypes[2] == AssetGfxWorld::EnumEntry);
        REQUIRE(assetTypes[3] == AssetGameWorldMp::EnumEntry);
        REQUIRE(assetTypes[4] == AssetMapEnts::EnumEntry);
    }

    TEST_CASE("T6 map emission preserves preloaded mapents", "[t6][map]")
    {
        Zone zone("zm_example", 0, GameId::T6, GamePlatform::PC);
        AssetCreatorCollection creatorCollection(zone);
        IgnoredAssetLookup ignoredAssetLookup;
        AssetCreationContext context(zone, &creatorCollection, &ignoredAssetLookup);

        const auto assetName = map::GetT6MapWorldAssetName(zone.m_name);
        const auto preloadedEntityString = "{\n\"classname\" \"stock_worldspawn\"\n}\n"s;
        auto* preloadedMapEnts = zone.Memory().Alloc<MapEnts>();
        preloadedMapEnts->name = zone.Memory().Dup(assetName.c_str());
        preloadedMapEnts->entityString = zone.Memory().Dup(preloadedEntityString.c_str());
        preloadedMapEnts->numEntityChars = static_cast<int>(preloadedEntityString.length() + 1u);
        preloadedMapEnts->trigger.count = 0u;
        preloadedMapEnts->trigger.models = nullptr;
        preloadedMapEnts->trigger.hullCount = 0u;
        preloadedMapEnts->trigger.hulls = nullptr;
        preloadedMapEnts->trigger.slabCount = 0u;
        preloadedMapEnts->trigger.slabs = nullptr;
        context.AddAsset<AssetMapEnts>(assetName, preloadedMapEnts);

        map::T6MapEntitySource entitySource;
        entitySource.m_entity_string = "{\n\"classname\" \"custom_worldspawn\"\n}\n";

        REQUIRE(map::EmitBaseWorldAssetsT6(context, zone, zone.m_name, entitySource, ZoneDefinitionMapType::ZM));

        const auto* mapEntsInfo = zone.m_pools.GetAsset<AssetMapEnts>(assetName);
        REQUIRE(mapEntsInfo != nullptr);
        REQUIRE(mapEntsInfo->Asset() == preloadedMapEnts);
        REQUIRE(std::string(mapEntsInfo->Asset()->entityString).find("stock_worldspawn") != std::string::npos);
        REQUIRE(zone.m_pools.GetAsset<AssetGameWorldMp>(assetName) != nullptr);
        REQUIRE(zone.m_pools.GetAsset<AssetGameWorldSp>(assetName) == nullptr);
    }
} // namespace
