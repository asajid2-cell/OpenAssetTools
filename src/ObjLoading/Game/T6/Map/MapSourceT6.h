#pragma once

#include "SearchPath/ISearchPath.h"
#include "Zone/Definition/ZoneDefinition.h"

#include <optional>
#include <array>
#include <string>
#include <vector>

namespace map
{
    struct T6MapPathNode
    {
        std::array<float, 3> m_origin{};
        float m_yaw = 0.0f;
        int m_spawn_flags = 0;
    };

    struct T6MapEntitySource
    {
        std::string m_entity_string;
        std::vector<T6MapPathNode> m_path_nodes;
        std::vector<std::string> m_material_dependencies;
        std::vector<std::string> m_rawfile_dependencies;
        std::vector<std::string> m_xmodel_dependencies;
        std::vector<std::string> m_zbarrier_dependencies;
    };

    [[nodiscard]] std::optional<T6MapEntitySource> LoadEntitySourceT6(ISearchPath& searchPath, ZoneDefinitionMapType mapType);
}
