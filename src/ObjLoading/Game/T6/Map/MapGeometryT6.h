#pragma once

#include "Game/T6/T6.h"
#include "SearchPath/ISearchPath.h"
#include "Zone/Definition/ZoneDefinition.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace map
{
    enum class T6MapMaterialType : std::uint8_t
    {
        Empty,
        Texture,
    };

    struct T6MapVertex
    {
        T6::vec3_t m_position;
        T6::vec4_t m_color;
        T6::vec2_t m_tex_coord;
        T6::vec3_t m_normal;
        T6::vec3_t m_tangent;
    };

    struct T6MapMaterial
    {
        T6MapMaterialType m_type;
        std::string m_name;
    };

    struct T6MapSurface
    {
        T6MapMaterial m_material;
        unsigned m_tri_count;
        unsigned m_first_vertex;
        unsigned m_first_index;
    };

    struct T6MapWorldGeometry
    {
        std::vector<T6MapSurface> m_surfaces;
        std::vector<T6MapVertex> m_vertices;
        std::vector<std::uint16_t> m_indices;
    };

    struct T6MapGeometry
    {
        std::string m_map_name;
        std::string m_world_asset_name;
        ZoneDefinitionMapType m_map_type;
        T6MapWorldGeometry m_gfx_world;
        T6MapWorldGeometry m_collision_world;
    };

    [[nodiscard]] std::string GetT6MapSourceFileName(const std::string& assetName);
    [[nodiscard]] std::unique_ptr<T6MapGeometry> LoadMapGeometryT6(ISearchPath& searchPath,
                                                                    const std::string& mapName,
                                                                    ZoneDefinitionMapType mapType);
}
