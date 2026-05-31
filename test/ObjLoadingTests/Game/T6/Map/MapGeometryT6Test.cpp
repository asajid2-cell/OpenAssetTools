#include "Game/T6/Map/MapGeometryT6.h"
#include "OatTestPaths.h"
#include "SearchPath/MockSearchPath.h"
#include "SearchPath/SearchPathFilesystem.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    TEST_CASE("T6 map source file names are resolved from bsp folder", "[t6][map]")
    {
        REQUIRE(map::GetT6MapSourceFileName("map_gfx.fbx") == "bsp/map_gfx.fbx");
    }

    TEST_CASE("T6 map geometry loads graphics FBX and reuses it for collision by default", "[t6][map]")
    {
        const auto testPath = oat::paths::GetTestDirectory() / "SystemTests/Game/T6/CustomMapPlumbing/ValidSourceMarkers";
        SearchPathFilesystem searchPath(testPath.string());

        const auto geometry = map::LoadMapGeometryT6(searchPath, "zm_custom_map_shell", ZoneDefinitionMapType::ZM);

        REQUIRE(geometry);
        REQUIRE(geometry->m_map_name == "zm_custom_map_shell");
        REQUIRE(geometry->m_world_asset_name == "maps/mp/zm_custom_map_shell.d3dbsp");
        REQUIRE(geometry->m_map_type == ZoneDefinitionMapType::ZM);

        REQUIRE_FALSE(geometry->m_gfx_world.m_surfaces.empty());
        REQUIRE_FALSE(geometry->m_gfx_world.m_vertices.empty());
        REQUIRE_FALSE(geometry->m_gfx_world.m_indices.empty());
        REQUIRE(geometry->m_gfx_world.m_indices.size() % 3u == 0u);

        REQUIRE(geometry->m_collision_world.m_surfaces.size() == geometry->m_gfx_world.m_surfaces.size());
        REQUIRE(geometry->m_collision_world.m_indices.size() % 3u == 0u);
        REQUIRE(geometry->m_gfx_world.m_surfaces[0].m_tri_count == geometry->m_collision_world.m_surfaces[0].m_tri_count * 2u);
        REQUIRE(geometry->m_gfx_world.m_indices.size() == geometry->m_collision_world.m_indices.size() * 2u);
        REQUIRE(geometry->m_gfx_world.m_vertices.size() >= geometry->m_collision_world.m_vertices.size());
    }

    TEST_CASE("T6 map geometry rejects invalid graphics FBX", "[t6][map]")
    {
        MockSearchPath searchPath;
        searchPath.AddFileData("bsp/map_gfx.fbx", "not an fbx");

        const auto geometry = map::LoadMapGeometryT6(searchPath, "zm_invalid", ZoneDefinitionMapType::ZM);

        REQUIRE_FALSE(geometry);
    }
} // namespace
