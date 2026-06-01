#include "Game/T6/Map/MapSourceT6.h"
#include "SearchPath/MockSearchPath.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    constexpr auto VALID_ENTITIES = R"json({
  "entities": [
    {
      "classname": "worldspawn",
      "lightgridoffset": "12",
      "lutmaterial": "Zm_nuketown_lut",
      "fogtime": "1",
      "fsi": "zm_nuked",
      "wsi": "zm_nuked",
      "skyboxmodel": "skybox_dlc0_zm_nuketown",
      "newsun": "1",
      "lightingquality": "8000",
      "removeredundantlinks": "0",
      "guid": "C05C0D01"
    },
    {
      "classname": "info_player_start",
      "origin": "0 0 0",
      "angles": "0 0 0",
      "guid": "C05C0D02"
    }
  ]
})json";

    TEST_CASE("T6 map entity source loads authored zombie entities", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/entities.json", VALID_ENTITIES);

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE(result);
        REQUIRE(result->m_entity_string.find(R"("classname" "worldspawn")") != std::string::npos);
        REQUIRE(result->m_entity_string.find(R"("classname" "info_player_start")") != std::string::npos);
        REQUIRE(result->m_material_dependencies.size() == 1u);
        REQUIRE(result->m_material_dependencies[0] == "zm_nuketown_lut");
        REQUIRE(result->m_rawfile_dependencies.size() == 1u);
        REQUIRE(result->m_rawfile_dependencies[0] == "vision/zm_nuked.vision");
        REQUIRE(result->m_xmodel_dependencies.size() == 1u);
        REQUIRE(result->m_xmodel_dependencies[0] == "skybox_dlc0_zm_nuketown");
        REQUIRE(result->m_zbarrier_dependencies.empty());
    }

    TEST_CASE("T6 zombie map entity source requires authored entities", "[t6][map]")
    {
        MockSearchPath searchPath;

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE_FALSE(result);
    }

    TEST_CASE("T6 map entity source requires first entity to be worldspawn", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/entities.json",
                               R"json({
  "entities": [
    {
      "classname": "info_player_start"
    }
  ]
})json");

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE_FALSE(result);
    }

    TEST_CASE("T6 map entity source requires string entity values", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/entities.json",
                               R"json({
  "entities": [
    {
      "classname": "worldspawn",
      "origin": [0, 0, 0]
    }
  ]
})json");

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE_FALSE(result);
    }

    TEST_CASE("T6 zombie map entity source requires runtime worldspawn keys", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/entities.json",
                               R"json({
  "entities": [
    {
      "classname": "worldspawn",
      "guid": "C05C0D01"
    }
  ]
})json");

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE_FALSE(result);
    }

    TEST_CASE("T6 zombie map entity source requires entity GUIDs", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/entities.json",
                               R"json({
  "entities": [
    {
      "classname": "worldspawn",
      "lightgridoffset": "12",
      "lutmaterial": "Zm_nuketown_lut",
      "fogtime": "1",
      "fsi": "zm_nuked",
      "wsi": "zm_nuked",
      "skyboxmodel": "skybox_dlc0_zm_nuketown",
      "newsun": "1",
      "lightingquality": "8000",
      "removeredundantlinks": "0",
      "guid": "C05C0D01"
    },
    {
      "classname": "info_player_start",
      "origin": "0 0 0",
      "angles": "0 0 0"
    }
  ]
})json");

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE_FALSE(result);
    }

    TEST_CASE("T6 zombie map entity source extracts zbarrier dependencies", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/entities.json",
                               R"json({
  "entities": [
    {
      "classname": "worldspawn",
      "lightgridoffset": "12",
      "lutmaterial": "Zm_nuketown_lut",
      "fogtime": "1",
      "fsi": "zm_nuked",
      "wsi": "zm_nuked",
      "skyboxmodel": "skybox_dlc0_zm_nuketown",
      "newsun": "1",
      "lightingquality": "8000",
      "removeredundantlinks": "0",
      "guid": "C05C0D01"
    },
    {
      "classname": "script_struct",
      "script_noteworthy_zbarrier": "barrier_window",
      "guid": "C05C0D03"
    }
  ]
})json");

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE(result);
        REQUIRE(result->m_zbarrier_dependencies.size() == 1u);
        REQUIRE(result->m_zbarrier_dependencies[0] == "barrier_window");
    }

    TEST_CASE("T6 zombie map entity source extracts authored path nodes", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/entities.json",
                               R"json({
  "entities": [
    {
      "classname": "worldspawn",
      "lightgridoffset": "12",
      "lutmaterial": "Zm_nuketown_lut",
      "fogtime": "1",
      "fsi": "zm_nuked",
      "wsi": "zm_nuked",
      "skyboxmodel": "skybox_dlc0_zm_nuketown",
      "newsun": "1",
      "lightingquality": "8000",
      "removeredundantlinks": "0",
      "guid": "C05C0D01"
    },
    {
      "classname": "node_pathnode",
      "origin": "128 256 64",
      "angles": "0 90 0",
      "spawnflags": "4",
      "guid": "C05C0D04"
    }
  ]
})json");

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE(result);
        REQUIRE(result->m_path_nodes.size() == 1u);
        REQUIRE(result->m_path_nodes[0].m_origin[0] == 128.0f);
        REQUIRE(result->m_path_nodes[0].m_origin[1] == 256.0f);
        REQUIRE(result->m_path_nodes[0].m_origin[2] == 64.0f);
        REQUIRE(result->m_path_nodes[0].m_yaw == 90.0f);
        REQUIRE(result->m_path_nodes[0].m_spawn_flags == 4);
    }

    TEST_CASE("T6 map entity source injects generated brush model references", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/entities.json",
                               R"json({
  "entities": [
    {
      "classname": "worldspawn",
      "lightgridoffset": "12",
      "lutmaterial": "Zm_nuketown_lut",
      "fogtime": "1",
      "fsi": "zm_nuked",
      "wsi": "zm_nuked",
      "skyboxmodel": "skybox_dlc0_zm_nuketown",
      "newsun": "1",
      "lightingquality": "8000",
      "removeredundantlinks": "0",
      "guid": "C05C0D01"
    },
    {
      "classname": "info_volume",
      "targetname": "test_zone",
      "origin": "0 0 96",
      "box_mins": "-128 -128 -96",
      "box_maxs": "128 128 160",
      "brush_contents": "1",
      "brush_surfaceflags": "4",
      "guid": "C05C0D05"
    }
  ]
})json");

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE(result);
        REQUIRE(result->m_brush_models.size() == 1u);
        REQUIRE(result->m_brush_models[0].m_mins[0] == -128.0f);
        REQUIRE(result->m_brush_models[0].m_mins[1] == -128.0f);
        REQUIRE(result->m_brush_models[0].m_mins[2] == -96.0f);
        REQUIRE(result->m_brush_models[0].m_maxs[0] == 128.0f);
        REQUIRE(result->m_brush_models[0].m_maxs[1] == 128.0f);
        REQUIRE(result->m_brush_models[0].m_maxs[2] == 160.0f);
        REQUIRE(result->m_brush_models[0].m_contents == 1);
        REQUIRE(result->m_brush_models[0].m_surface_flags == 4);
        REQUIRE(result->m_entity_string.find(R"("model" "*1")") != std::string::npos);
        REQUIRE(result->m_entity_string.find("box_mins") == std::string::npos);
        REQUIRE(result->m_entity_string.find("box_maxs") == std::string::npos);
        REQUIRE(result->m_entity_string.find("brush_contents") == std::string::npos);
    }

    TEST_CASE("T6 map entity source rejects partial generated brush model bounds", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/entities.json",
                               R"json({
  "entities": [
    {
      "classname": "worldspawn",
      "lightgridoffset": "12",
      "lutmaterial": "Zm_nuketown_lut",
      "fogtime": "1",
      "fsi": "zm_nuked",
      "wsi": "zm_nuked",
      "skyboxmodel": "skybox_dlc0_zm_nuketown",
      "newsun": "1",
      "lightingquality": "8000",
      "removeredundantlinks": "0",
      "guid": "C05C0D01"
    },
    {
      "classname": "info_volume",
      "box_mins": "-128 -128 -96",
      "guid": "C05C0D05"
    }
  ]
})json");

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE_FALSE(result);
    }

    TEST_CASE("T6 map entity source defaults trigger brush models to stock touch-volume contents", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/entities.json",
                               R"json({
  "entities": [
    {
      "classname": "worldspawn",
      "lightgridoffset": "12",
      "lutmaterial": "Zm_nuketown_lut",
      "fogtime": "1",
      "fsi": "zm_nuked",
      "wsi": "zm_nuked",
      "skyboxmodel": "skybox_dlc0_zm_nuketown",
      "newsun": "1",
      "lightingquality": "8000",
      "removeredundantlinks": "0",
      "guid": "C05C0D01"
    },
    {
      "classname": "info_volume",
      "box_mins": "-128 -128 -96",
      "box_maxs": "128 128 160",
      "guid": "C05C0D05"
    },
    {
      "classname": "trigger_use_touch",
      "box_mins": "-16 -16 -16",
      "box_maxs": "16 16 16",
      "guid": "C05C0D06"
    }
  ]
})json");

        const auto result = map::LoadEntitySourceT6(searchPath, ZoneDefinitionMapType::ZM);

        REQUIRE(result);
        REQUIRE(result->m_brush_models.size() == 2u);
        REQUIRE(result->m_brush_models[0].m_contents == 0x08000001);
        REQUIRE(result->m_brush_models[0].m_surface_flags == 0);
        REQUIRE(result->m_brush_models[1].m_contents == 0x08000001);
        REQUIRE(result->m_brush_models[1].m_surface_flags == 0);
    }
} // namespace
