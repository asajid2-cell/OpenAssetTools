#include "Asset/AssetCreationContext.h"
#include "Asset/AssetCreatorCollection.h"
#include "Game/T6/Map/MapGeometryT6.h"
#include "Game/T6/Map/MapGfxWorldT6.h"
#include "Game/T6/T6.h"
#include "OatTestPaths.h"
#include "SearchPath/SearchPathFilesystem.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using namespace T6;

namespace
{
    constexpr const char* WORLD_FALLBACK_MATERIAL = "wpc/wood_planks_old_white";
    constexpr const char* WORLD_TECHNIQUE_SET = "wpc_lit_sm_r0c0n0_80fe30wz";

    void AddMaterial(AssetCreationContext& context,
                     MemoryManager& memory,
                     const std::string& name,
                     const unsigned int materialSortedIndex = 17u,
                     const std::string& techniqueSetName = WORLD_TECHNIQUE_SET)
    {
        auto* material = memory.Alloc<Material>();
        material->info.name = memory.Dup(name.c_str());
        material->info.drawSurf.fields.objectId = 777u;
        material->info.drawSurf.fields.customIndex = 31u;
        material->info.drawSurf.fields.reflectionProbeIndex = 7u;
        material->info.drawSurf.fields.materialSortedIndex = materialSortedIndex;
        material->info.drawSurf.fields.primaryLightIndex = 99u;
        material->info.drawSurf.fields.surfType = 0u;
        material->info.drawSurf.fields.prepass = 2u;
        material->info.drawSurf.fields.primarySortKey = 4u;
        auto* techniqueSet = memory.Alloc<MaterialTechniqueSet>();
        std::memset(techniqueSet, 0, sizeof(MaterialTechniqueSet));
        techniqueSet->name = memory.Dup(techniqueSetName.c_str());
        material->techniqueSet = techniqueSet;
        context.AddAsset<AssetMaterial>(name, material);
    }

    void AddImage(AssetCreationContext& context, MemoryManager& memory, const std::string& name)
    {
        auto* image = memory.Alloc<GfxImage>();
        image->name = memory.Dup(name.c_str());
        context.AddAsset<AssetImage>(name, image);
    }

    void AssertRuntimeReadyMapImage(const GfxImage* image, const MapType mapType, const ImageCategory category)
    {
        REQUIRE(image != nullptr);
        REQUIRE(image->texture.loadDef != nullptr);
        REQUIRE(image->mapType == mapType);
        REQUIRE(image->category == category);
        REQUIRE(image->delayLoadPixels == false);
        REQUIRE(image->streaming == 0);
        REQUIRE(image->streamedPartCount == 0);
        REQUIRE(image->pixels == nullptr);
        REQUIRE(image->baseSize > 0u);
        REQUIRE(image->loadedSize == image->baseSize);
        REQUIRE(image->cardMemory.platform[0] == static_cast<int>(image->baseSize));
        REQUIRE(image->cardMemory.platform[1] == static_cast<int>(image->baseSize));
        REQUIRE(image->levelCount == image->texture.loadDef->levelCount);
        REQUIRE(image->hash != 0u);
    }

    void AssertGeneratedImagePixel(const GfxImage* image, const unsigned char red, const unsigned char green, const unsigned char blue)
    {
        REQUIRE(image != nullptr);
        REQUIRE(image->texture.loadDef != nullptr);
        REQUIRE(image->texture.loadDef->resourceSize >= 4);
        REQUIRE(static_cast<unsigned char>(image->texture.loadDef->data[0]) == red);
        REQUIRE(static_cast<unsigned char>(image->texture.loadDef->data[1]) == green);
        REQUIRE(static_cast<unsigned char>(image->texture.loadDef->data[2]) == blue);
        REQUIRE(static_cast<unsigned char>(image->texture.loadDef->data[3]) == 255u);
    }

    void SetPosition(map::T6MapVertex& vertex, const float x, const float y, const float z)
    {
        vertex.m_position.x = x;
        vertex.m_position.y = y;
        vertex.m_position.z = z;
    }

    TEST_CASE("T6 map GfxWorld emits draw data from imported geometry", "[t6][map]")
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
        AddMaterial(context, memory, WORLD_FALLBACK_MATERIAL);
        AddImage(context, memory, "reflection_probe0");
        AddImage(context, memory, "lightmap0_secondary");
        AddImage(context, memory, "$outdoor");

        const auto* gfxWorld = map::CreateGfxWorldT6(memory, searchPath, context, *geometry);

        REQUIRE(gfxWorld != nullptr);
        REQUIRE(std::string(gfxWorld->name) == "maps/mp/zm_custom_map_shell.d3dbsp");
        REQUIRE(std::string(gfxWorld->baseName) == "zm_custom_map_shell");
        REQUIRE(gfxWorld->surfaceCount == static_cast<int>(geometry->m_gfx_world.m_surfaces.size()));
        REQUIRE(gfxWorld->draw.vertexCount == geometry->m_gfx_world.m_vertices.size());
        REQUIRE(gfxWorld->draw.indexCount == static_cast<int>(geometry->m_gfx_world.m_indices.size()));
        const auto vertexDataBytes = geometry->m_gfx_world.m_vertices.size() * sizeof(GfxPackedWorldVertex);
        REQUIRE(gfxWorld->draw.vertexDataSize0 == static_cast<unsigned int>(vertexDataBytes));
        REQUIRE(gfxWorld->draw.vertexDataSize1 == 0x20u);
        REQUIRE(gfxWorld->dpvs.surfaces != nullptr);
        REQUIRE(gfxWorld->dpvs.surfaces[0].tris.firstVertex == 0);
        REQUIRE(gfxWorld->dpvs.surfaces[0].tris.vertexCount == geometry->m_gfx_world.m_vertices.size());
        REQUIRE(gfxWorld->dpvs.surfaces[0].tris.himipRadiusInvSq == 0.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[0].tris.mins.x == gfxWorld->dpvs.surfaces[0].bounds[0].x);
        REQUIRE(gfxWorld->dpvs.surfaces[0].tris.mins.y == gfxWorld->dpvs.surfaces[0].bounds[0].y);
        REQUIRE(gfxWorld->dpvs.surfaces[0].tris.mins.z == gfxWorld->dpvs.surfaces[0].bounds[0].z);
        REQUIRE(gfxWorld->dpvs.surfaces[0].tris.maxs.x == gfxWorld->dpvs.surfaces[0].bounds[1].x);
        REQUIRE(gfxWorld->dpvs.surfaces[0].tris.maxs.y == gfxWorld->dpvs.surfaces[0].bounds[1].y);
        REQUIRE(gfxWorld->dpvs.surfaces[0].tris.maxs.z == gfxWorld->dpvs.surfaces[0].bounds[1].z);
        REQUIRE(gfxWorld->dpvs.surfaces[0].material != nullptr);
        REQUIRE(std::string(gfxWorld->dpvs.surfaces[0].material->info.name) == WORLD_FALLBACK_MATERIAL);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials != nullptr);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials[0].fields.objectId == 0u);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials[0].fields.customIndex == 0u);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials[0].fields.reflectionProbeIndex == 0u);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials[0].fields.materialSortedIndex == 17u);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials[0].fields.primaryLightIndex == 1u);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials[0].fields.prepass == 2u);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials[0].fields.primarySortKey == 4u);
        REQUIRE(gfxWorld->draw.vd0.data != nullptr);
        REQUIRE(gfxWorld->draw.vd1.data != nullptr);
        REQUIRE(gfxWorld->draw.indices != nullptr);
        REQUIRE(gfxWorld->modelCount == 1);
        REQUIRE(gfxWorld->primaryLightCount == 2u);
        REQUIRE(gfxWorld->sunLight != nullptr);
        REQUIRE(gfxWorld->sunLight->type == GFX_LIGHT_TYPE_DIR);
        REQUIRE(gfxWorld->sunLight->dir.x == -0.242f);
        REQUIRE(gfxWorld->sunLight->dir.y == -0.841f);
        REQUIRE(gfxWorld->sunLight->dir.z == -0.485f);
        REQUIRE(gfxWorld->draw.lightmapCount == 1);
        REQUIRE(gfxWorld->draw.lightmaps[0].primary != nullptr);
        REQUIRE(std::string(gfxWorld->draw.lightmaps[0].primary->name) == "zm_custom_map_shell_lightmap0");
        AssertRuntimeReadyMapImage(gfxWorld->draw.lightmaps[0].primary, MAPTYPE_2D, IMG_CATEGORY_LIGHTMAP);
        AssertGeneratedImagePixel(gfxWorld->draw.lightmaps[0].primary, 255u, 255u, 255u);
        REQUIRE(gfxWorld->draw.lightmaps[0].secondary != nullptr);
        REQUIRE(std::string(gfxWorld->draw.lightmaps[0].secondary->name) == "zm_custom_map_shell_lightmap0_secondary");
        AssertRuntimeReadyMapImage(gfxWorld->draw.lightmaps[0].secondary, MAPTYPE_2D, IMG_CATEGORY_LIGHTMAP);
        AssertGeneratedImagePixel(gfxWorld->draw.lightmaps[0].secondary, 255u, 255u, 255u);
        REQUIRE(gfxWorld->draw.reflectionProbeCount == 1u);
        REQUIRE(gfxWorld->draw.reflectionProbes[0].reflectionImage != nullptr);
        REQUIRE(std::string(gfxWorld->draw.reflectionProbes[0].reflectionImage->name) == "zm_custom_map_shell_reflection_probe0");
        AssertRuntimeReadyMapImage(gfxWorld->draw.reflectionProbes[0].reflectionImage, MAPTYPE_CUBE, IMG_CATEGORY_AUTO_GENERATED);
        AssertGeneratedImagePixel(gfxWorld->draw.reflectionProbes[0].reflectionImage, 128u, 128u, 128u);
        REQUIRE(gfxWorld->draw.reflectionProbes[0].probeVolumeCount == 0u);
        REQUIRE(gfxWorld->draw.reflectionProbes[0].probeVolumes == nullptr);
        REQUIRE(gfxWorld->draw.reflectionProbeTextures[0].basemap == nullptr);
        REQUIRE(gfxWorld->materialMemoryCount == 1);
        REQUIRE(gfxWorld->materialMemory != nullptr);
        REQUIRE(gfxWorld->materialMemory[0].material == gfxWorld->dpvs.surfaces[0].material);
        REQUIRE(gfxWorld->outdoorImage != nullptr);
        REQUIRE(std::string(gfxWorld->outdoorImage->name) == "$outdoor");
        AssertRuntimeReadyMapImage(gfxWorld->outdoorImage, MAPTYPE_2D, IMG_CATEGORY_AUTO_GENERATED);
        REQUIRE(gfxWorld->dpvs.smodelCount == 0u);
        REQUIRE(gfxWorld->dpvs.smodelInsts != nullptr);
        REQUIRE(gfxWorld->dpvs.smodelDrawInsts != nullptr);
        REQUIRE(gfxWorld->dpvs.smodelVisDataCount == 0u);
        REQUIRE(gfxWorld->dpvs.smodelVisData[0] != nullptr);
        REQUIRE(gfxWorld->dpvs.smodelVisData[1] != nullptr);
        REQUIRE(gfxWorld->dpvs.smodelVisData[2] != nullptr);
        REQUIRE(gfxWorld->dpvs.smodelVisDataCameraSaved != nullptr);
        REQUIRE(gfxWorld->dpvs.smodelCastsShadow != nullptr);
        REQUIRE(gfxWorld->nodeCount == 1);
        REQUIRE(gfxWorld->planeCount == 0);
        REQUIRE(gfxWorld->dpvsPlanes.nodes != nullptr);
        REQUIRE(gfxWorld->dpvsPlanes.nodes[0] == 1u);
        REQUIRE(gfxWorld->dpvsPlanes.planes == nullptr);
        REQUIRE(gfxWorld->dpvsPlanes.cellCount == 1);
        REQUIRE(gfxWorld->dpvs.surfaceVisData[0][0] == static_cast<raw_byte128>(0xFF));
        REQUIRE(gfxWorld->dpvs.surfaceVisData[1][0] == static_cast<raw_byte128>(0xFF));
        REQUIRE(gfxWorld->dpvs.surfaceVisData[2][0] == static_cast<raw_byte128>(0xFF));
        REQUIRE(gfxWorld->cells[0].aabbTree[0].smodelIndexCount == 0u);
        REQUIRE(gfxWorld->cells[0].aabbTree[0].smodelIndexes != nullptr);
        REQUIRE(gfxWorld->skyDynIntensity.factor0 == 1.0f);
        REQUIRE(gfxWorld->skyDynIntensity.factor1 == 1.0f);
        REQUIRE(gfxWorld->dpvsDyn.dynEntClientCount[0] == 256u);
        REQUIRE(gfxWorld->dpvsDyn.dynEntClientWordCount[0] == 8u);
        REQUIRE(gfxWorld->dpvsDyn.dynEntCellBits[0] != nullptr);
        REQUIRE(gfxWorld->dpvsDyn.dynEntVisData[0][0] != nullptr);
        REQUIRE(gfxWorld->sceneDynModel != nullptr);
        REQUIRE(gfxWorld->lightGrid.maxs[0] == 200u);
        REQUIRE(gfxWorld->lightGrid.maxs[1] == 200u);
        REQUIRE(gfxWorld->lightGrid.maxs[2] == 50u);
        REQUIRE(gfxWorld->lightGrid.rowDataStart != nullptr);
        REQUIRE(gfxWorld->lightGrid.rowDataStart[0] == 0u);
        REQUIRE(gfxWorld->lightGrid.rawRowData != nullptr);
        const auto* lightGridRow = reinterpret_cast<const GfxLightGridRow*>(gfxWorld->lightGrid.rawRowData);
        REQUIRE(lightGridRow->colCount == 0x1000u);
        REQUIRE(lightGridRow->zCount == 0xFFu);
        REQUIRE(gfxWorld->lightGrid.entryCount == 60000u);
        REQUIRE(gfxWorld->lightGrid.colorCount == 0x1000u);
        REQUIRE(gfxWorld->lightGrid.colors != nullptr);
        REQUIRE(gfxWorld->lightGrid.coeffCount == 0u);
        REQUIRE(gfxWorld->lightGrid.coeffs == nullptr);
    }

    TEST_CASE("T6 map GfxWorld rejects model-material families for world surfaces", "[t6][map]")
    {
        const auto testPath = oat::paths::GetTestDirectory() / "SystemTests/Game/T6/CustomMapPlumbing/ValidSourceMarkers";
        SearchPathFilesystem searchPath(testPath.string());

        auto geometry = map::LoadMapGeometryT6(searchPath, "zm_custom_map_shell", ZoneDefinitionMapType::ZM);
        REQUIRE(geometry);
        REQUIRE(!geometry->m_gfx_world.m_surfaces.empty());

        geometry->m_gfx_world.m_surfaces[0].m_material = {map::T6MapMaterialType::Texture, "mc/bad_model_material"};

        Zone zone("zm_custom_map_shell", 0, GameId::T6, GamePlatform::PC);
        AssetCreatorCollection creatorCollection(zone);
        IgnoredAssetLookup ignoredAssetLookup;
        AssetCreationContext context(zone, &creatorCollection, &ignoredAssetLookup);
        MemoryManager memory;

        AddMaterial(context, memory, "mc/bad_model_material", 17u, "mc_lit_sm_r0c0n0x0_q361191u");
        AddMaterial(context, memory, WORLD_FALLBACK_MATERIAL);
        AddImage(context, memory, "reflection_probe0");
        AddImage(context, memory, "lightmap0_secondary");
        AddImage(context, memory, "$outdoor");

        REQUIRE(map::CreateGfxWorldT6(memory, searchPath, context, *geometry) == nullptr);
    }

    TEST_CASE("T6 map GfxWorld generates an outdoor image when the dependency is ignored", "[t6][map]")
    {
        const auto testPath = oat::paths::GetTestDirectory() / "SystemTests/Game/T6/CustomMapPlumbing/ValidSourceMarkers";
        SearchPathFilesystem searchPath(testPath.string());

        const auto geometry = map::LoadMapGeometryT6(searchPath, "zm_custom_map_shell", ZoneDefinitionMapType::ZM);
        REQUIRE(geometry);

        AssetList ignoredAssets;
        ignoredAssets.m_entries.emplace_back(AssetImage::EnumEntry, "$outdoor", false);

        Zone zone("zm_custom_map_shell", 0, GameId::T6, GamePlatform::PC);
        AssetCreatorCollection creatorCollection(zone);
        IgnoredAssetLookup ignoredAssetLookup(ignoredAssets);
        AssetCreationContext context(zone, &creatorCollection, &ignoredAssetLookup);
        MemoryManager memory;

        AddMaterial(context, memory, WORLD_FALLBACK_MATERIAL);
        AddImage(context, memory, "reflection_probe0");
        AddImage(context, memory, "lightmap0_secondary");

        const auto* gfxWorld = map::CreateGfxWorldT6(memory, searchPath, context, *geometry);

        REQUIRE(gfxWorld != nullptr);
        REQUIRE(gfxWorld->outdoorImage != nullptr);
        REQUIRE(std::string(gfxWorld->outdoorImage->name) == "$outdoor");
        REQUIRE(!zone.m_pools.GetAsset<AssetImage>(",$outdoor"));
    }

    TEST_CASE("T6 map GfxWorld generates map-private images when none are target-local", "[t6][map]")
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

        AddMaterial(context, memory, WORLD_FALLBACK_MATERIAL);

        const auto* gfxWorld = map::CreateGfxWorldT6(memory, searchPath, context, *geometry);

        REQUIRE(gfxWorld != nullptr);
        REQUIRE(gfxWorld->draw.lightmaps[0].secondary != nullptr);
        REQUIRE(std::string(gfxWorld->draw.lightmaps[0].secondary->name) == "zm_custom_map_shell_lightmap0_secondary");
        AssertRuntimeReadyMapImage(gfxWorld->draw.lightmaps[0].secondary, MAPTYPE_2D, IMG_CATEGORY_LIGHTMAP);
        REQUIRE(gfxWorld->draw.reflectionProbes[0].reflectionImage != nullptr);
        REQUIRE(std::string(gfxWorld->draw.reflectionProbes[0].reflectionImage->name) == "zm_custom_map_shell_reflection_probe0");
        AssertRuntimeReadyMapImage(gfxWorld->draw.reflectionProbes[0].reflectionImage, MAPTYPE_CUBE, IMG_CATEGORY_AUTO_GENERATED);
        REQUIRE(gfxWorld->outdoorImage != nullptr);
        REQUIRE(std::string(gfxWorld->outdoorImage->name) == "$outdoor");
        AssertRuntimeReadyMapImage(gfxWorld->outdoorImage, MAPTYPE_2D, IMG_CATEGORY_AUTO_GENERATED);
    }

    TEST_CASE("T6 map GfxWorld bounds use byte vertex-data offsets", "[t6][map]")
    {
        const auto testPath = oat::paths::GetTestDirectory() / "SystemTests/Game/T6/CustomMapPlumbing/ValidSourceMarkers";
        SearchPathFilesystem searchPath(testPath.string());

        auto geometry = map::LoadMapGeometryT6(searchPath, "zm_custom_map_shell", ZoneDefinitionMapType::ZM);
        REQUIRE(geometry);
        REQUIRE(geometry->m_gfx_world.m_vertices.size() >= 6u);

        auto& world = geometry->m_gfx_world;
        world.m_surfaces.clear();
        world.m_indices = {0u, 1u, 2u, 0u, 1u, 2u};

        map::T6MapSurface firstSurface{};
        firstSurface.m_material = {map::T6MapMaterialType::Texture, "streaming_temp_image_0"};
        firstSurface.m_tri_count = 1u;
        firstSurface.m_first_vertex = 0u;
        firstSurface.m_first_index = 0u;
        world.m_surfaces.emplace_back(firstSurface);

        map::T6MapSurface secondSurface{};
        secondSurface.m_material = {map::T6MapMaterialType::Texture, "streaming_temp_image_0"};
        secondSurface.m_tri_count = 1u;
        secondSurface.m_first_vertex = 3u;
        secondSurface.m_first_index = 3u;
        world.m_surfaces.emplace_back(secondSurface);

        SetPosition(world.m_vertices[0], 0.0f, 0.0f, 0.0f);
        SetPosition(world.m_vertices[1], 10.0f, 0.0f, 0.0f);
        SetPosition(world.m_vertices[2], 0.0f, 20.0f, 0.0f);
        SetPosition(world.m_vertices[3], 100.0f, 200.0f, 300.0f);
        SetPosition(world.m_vertices[4], 150.0f, 200.0f, 300.0f);
        SetPosition(world.m_vertices[5], 100.0f, 250.0f, 350.0f);

        Zone zone("zm_custom_map_shell", 0, GameId::T6, GamePlatform::PC);
        AssetCreatorCollection creatorCollection(zone);
        IgnoredAssetLookup ignoredAssetLookup;
        AssetCreationContext context(zone, &creatorCollection, &ignoredAssetLookup);
        MemoryManager memory;

        AddMaterial(context, memory, "streaming_temp_image_0");
        AddMaterial(context, memory, WORLD_FALLBACK_MATERIAL);
        AddImage(context, memory, "reflection_probe0");
        AddImage(context, memory, "lightmap0_secondary");
        AddImage(context, memory, "$outdoor");

        const auto* gfxWorld = map::CreateGfxWorldT6(memory, searchPath, context, *geometry);

        REQUIRE(gfxWorld != nullptr);
        REQUIRE(gfxWorld->surfaceCount == 2);
        REQUIRE(gfxWorld->dpvs.surfaces[1].tris.firstVertex == 3);
        REQUIRE(gfxWorld->dpvs.surfaces[1].tris.vertexCount == 3u);
        REQUIRE(gfxWorld->dpvs.surfaces[1].tris.vertexDataOffset0 == static_cast<int>(3u * sizeof(GfxPackedWorldVertex)));
        REQUIRE(gfxWorld->dpvs.surfaces[1].tris.mins.x == 100.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].tris.mins.y == 200.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].tris.mins.z == 300.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].tris.maxs.x == 150.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].tris.maxs.y == 250.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].tris.maxs.z == 350.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].bounds[0].x == 100.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].bounds[0].y == 200.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].bounds[0].z == 300.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].bounds[1].x == 150.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].bounds[1].y == 250.0f);
        REQUIRE(gfxWorld->dpvs.surfaces[1].bounds[1].z == 350.0f);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials[1].fields.objectId == 1u);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials[1].fields.primaryLightIndex == 1u);
        REQUIRE(gfxWorld->dpvs.surfaceMaterials[1].fields.reflectionProbeIndex == 0u);
        REQUIRE(gfxWorld->maxs.x == 150.0f);
        REQUIRE(gfxWorld->maxs.y == 250.0f);
        REQUIRE(gfxWorld->maxs.z == 350.0f);
    }
} // namespace
