#include "Asset/AssetCreationContext.h"
#include "Asset/AssetCreatorCollection.h"
#include "Game/T6/Map/MapClipMapT6.h"
#include "Game/T6/Map/MapGeometryT6.h"
#include "Game/T6/Map/MapGfxWorldT6.h"
#include "Game/T6/T6.h"
#include "OatTestPaths.h"
#include "SearchPath/SearchPathFilesystem.h"

#include <catch2/catch_test_macros.hpp>

using namespace T6;

namespace
{
    void AddMaterial(AssetCreationContext& context, MemoryManager& memory, const std::string& name)
    {
        auto* material = memory.Alloc<Material>();
        material->info.name = memory.Dup(name.c_str());
        context.AddAsset<AssetMaterial>(name, material);
    }

    void AddImage(AssetCreationContext& context, MemoryManager& memory, const std::string& name)
    {
        auto* image = memory.Alloc<GfxImage>();
        image->name = memory.Dup(name.c_str());
        context.AddAsset<AssetImage>(name, image);
    }

    MapEnts* AddMapEnts(AssetCreationContext& context, MemoryManager& memory, const std::string& name)
    {
        auto* mapEnts = memory.Alloc<MapEnts>();
        mapEnts->name = memory.Dup(name.c_str());
        mapEnts->entityString = memory.Dup("{\n\"classname\" \"worldspawn\"\n}\n");
        mapEnts->numEntityChars = 30;
        context.AddAsset<AssetMapEnts>(name, mapEnts);
        return mapEnts;
    }

    TEST_CASE("T6 map ClipMap emits collision data from imported geometry", "[t6][map]")
    {
        const auto testPath = oat::paths::GetTestDirectory() / "SystemTests/Game/T6/CustomMapPlumbing/ValidSourceMarkers";
        SearchPathFilesystem searchPath(testPath.string());

        const auto geometry = map::LoadMapGeometryT6(searchPath, "zm_custom_map_shell", ZoneDefinitionMapType::ZM);
        REQUIRE(geometry);

        Zone zone("zm_custom_map_shell", 0, GameId::T6, GamePlatform::PC);
        AssetCreatorCollection creatorCollection(zone);
        IgnoredAssetLookup ignoredAssetLookup;
        AssetCreationContext context(zone, &creatorCollection, &ignoredAssetLookup);
        MemoryManager memory;

        AddMaterial(context, memory, "streaming_temp_image_0");
        AddMaterial(context, memory, "wpc/wood_planks_old_white");
        AddImage(context, memory, "reflection_probe0");
        AddImage(context, memory, "lightmap0_secondary");
        AddImage(context, memory, "$outdoor");
        const auto* mapEnts = AddMapEnts(context, memory, geometry->m_world_asset_name);

        const auto* gfxWorld = map::CreateGfxWorldT6(memory, searchPath, context, *geometry);
        REQUIRE(gfxWorld != nullptr);

        const auto* clipMap = map::CreateClipMapT6(memory, context, *geometry, *gfxWorld);

        REQUIRE(clipMap != nullptr);
        REQUIRE(std::string(clipMap->name) == "maps/mp/zm_custom_map_shell.d3dbsp");
        REQUIRE(clipMap->mapEnts == mapEnts);
        REQUIRE(clipMap->isInUse == 1);
        REQUIRE(clipMap->pInfo == nullptr);
        REQUIRE(clipMap->numStaticModels == 0u);
        REQUIRE(clipMap->staticModelList == nullptr);
        REQUIRE(clipMap->vertCount == geometry->m_collision_world.m_vertices.size());
        REQUIRE(clipMap->triCount == static_cast<int>(geometry->m_collision_world.m_indices.size() / 3u));
        REQUIRE(clipMap->partitionCount == clipMap->triCount);
        REQUIRE(clipMap->numSubModels == 1u);
        REQUIRE(clipMap->numNodes > 0u);
        REQUIRE(clipMap->numLeafs > 1u);
        REQUIRE(clipMap->leafs[0].leafBrushNode == 0);
        REQUIRE(clipMap->leafs[0].terrainContents == 0);
        REQUIRE(clipMap->leafs[0].firstCollAabbIndex == 0);
        REQUIRE(clipMap->leafs[0].collAabbCount == 0);
        REQUIRE(clipMap->leafs[0].mins.x == 0.0f);
        REQUIRE(clipMap->leafs[0].mins.y == 0.0f);
        REQUIRE(clipMap->leafs[0].mins.z == 0.0f);
        REQUIRE(clipMap->leafs[0].maxs.x == 0.0f);
        REQUIRE(clipMap->leafs[0].maxs.y == 0.0f);
        REQUIRE(clipMap->leafs[0].maxs.z == 0.0f);
        REQUIRE(clipMap->leafs[1].mins.x <= clipMap->leafs[1].maxs.x);
        REQUIRE(clipMap->leafs[1].mins.y <= clipMap->leafs[1].maxs.y);
        REQUIRE(clipMap->leafs[1].mins.z <= clipMap->leafs[1].maxs.z);
        REQUIRE((clipMap->leafs[1].mins.x != clipMap->leafs[1].maxs.x || clipMap->leafs[1].mins.y != clipMap->leafs[1].maxs.y
                 || clipMap->leafs[1].mins.z != clipMap->leafs[1].maxs.z));
        REQUIRE(clipMap->leafs[1].terrainContents == 1);
        REQUIRE(clipMap->leafs[1].brushContents == 134414848);
        REQUIRE(clipMap->cmodels[0].leaf.leafBrushNode == 0);
        REQUIRE(clipMap->cmodels[0].leaf.mins.x == 0.0f);
        REQUIRE(clipMap->cmodels[0].leaf.mins.y == 0.0f);
        REQUIRE(clipMap->cmodels[0].leaf.mins.z == 0.0f);
        REQUIRE(clipMap->cmodels[0].leaf.maxs.x == 0.0f);
        REQUIRE(clipMap->cmodels[0].leaf.maxs.y == 0.0f);
        REQUIRE(clipMap->cmodels[0].leaf.maxs.z == 0.0f);
        REQUIRE(clipMap->cmodels[0].info == nullptr);
        REQUIRE(clipMap->aabbTreeCount > 0);
        REQUIRE(clipMap->info.numMaterials == 1u);
        REQUIRE(clipMap->info.materials != nullptr);
        REQUIRE(std::string(clipMap->info.materials[0].name) == "light_demote_hint");
        REQUIRE(clipMap->info.materials[0].surfaceFlags == 278656);
        REQUIRE(clipMap->info.materials[0].contentFlags == 134217728);
        REQUIRE(clipMap->info.leafbrushNodesCount == 2u);
        REQUIRE(clipMap->info.leafbrushNodes != nullptr);
        REQUIRE(clipMap->info.numLeafBrushes == 1u);
        REQUIRE(clipMap->info.leafbrushes != nullptr);
        REQUIRE(clipMap->info.numBrushes == 1u);
        REQUIRE(clipMap->info.brushes != nullptr);
        REQUIRE(clipMap->info.brushBounds != nullptr);
        REQUIRE(clipMap->info.brushContents != nullptr);
        REQUIRE(clipMap->info.leafbrushNodes[0].axis == 0);
        REQUIRE(clipMap->info.leafbrushNodes[0].leafBrushCount == 0);
        REQUIRE(clipMap->info.leafbrushNodes[0].contents == 0);
        REQUIRE(clipMap->leafs[1].leafBrushNode == 1);
        REQUIRE(clipMap->info.leafbrushNodes[1].axis == 0);
        REQUIRE(clipMap->info.leafbrushNodes[1].leafBrushCount == 1);
        REQUIRE(clipMap->info.leafbrushNodes[1].contents == 134414848);
        REQUIRE(clipMap->info.leafbrushNodes[1].data.leaf.brushes == clipMap->info.leafbrushes);
        REQUIRE(clipMap->info.leafbrushNodes[1].data.leaf.brushes[0] == 0u);
        REQUIRE(clipMap->box_model.leaf.leafBrushNode == 1);
        REQUIRE(clipMap->box_brush != nullptr);
        REQUIRE(clipMap->box_brush->contents == -1);
        REQUIRE(clipMap->box_brush->numsides == 0u);
        REQUIRE(clipMap->box_brush->sides == nullptr);
        REQUIRE(clipMap->box_brush->numverts == 0u);
        REQUIRE(clipMap->box_brush->verts == nullptr);
        for (auto side = 0u; side < 2u; side++)
        {
            for (auto axis = 0u; axis < 3u; axis++)
            {
                REQUIRE(clipMap->box_brush->axial_cflags[side][axis] == -1);
                REQUIRE(clipMap->box_brush->axial_sflags[side][axis] == -1);
            }
        }
        REQUIRE(clipMap->info.brushes[0].contents == 134414848);
        REQUIRE(clipMap->info.brushContents[0] == 134414848);
        REQUIRE(clipMap->triEdgeIsWalkable != nullptr);
        REQUIRE(clipMap->originalDynEntCount == 0u);
        REQUIRE(clipMap->dynEntCount[0] == 256u);
        REQUIRE(clipMap->dynEntClientList[0] != nullptr);
        REQUIRE(clipMap->dynEntCollList[0] != nullptr);
        REQUIRE(clipMap->dynEntPoseList[0] != nullptr);
        REQUIRE(clipMap->dynEntDefList[0] != nullptr);
        REQUIRE(clipMap->dynEntServerList[0] == nullptr);
        REQUIRE(clipMap->clusterBytes == 136);
        REQUIRE(clipMap->visibility != nullptr);
    }
} // namespace
