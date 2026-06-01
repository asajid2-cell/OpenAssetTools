#include "MapSourceT6.h"

#include "Utils/Logging/Log.h"
#include "Utils/StringUtils.h"

#include <format>
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace nlohmann;

namespace
{
    constexpr auto ENTITY_FILE = "bsp/entities.json";
    constexpr auto DEFAULT_ENTITY_STRING = "{\n\"classname\" \"worldspawn\"\n}\n{\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 0\"\n\"angles\" \"0 0 0\"\n}\n";
    constexpr auto GENERATED_TOUCH_VOLUME_CONTENTS = 0x08000001;
    constexpr auto GENERATED_TOUCH_VOLUME_SURFACE_FLAGS = 0;

    [[nodiscard]] const char* GetMapTypeName(const ZoneDefinitionMapType mapType)
    {
        switch (mapType)
        {
        case ZoneDefinitionMapType::SP:
            return "SP";

        case ZoneDefinitionMapType::MP:
            return "MP";

        case ZoneDefinitionMapType::ZM:
            return "ZM";

        case ZoneDefinitionMapType::NONE:
        default:
            return "map";
        }
    }

    [[nodiscard]] bool RequiresAuthoredEntities(const ZoneDefinitionMapType mapType)
    {
        return mapType == ZoneDefinitionMapType::ZM;
    }

    [[nodiscard]] bool IsZombieMapType(const ZoneDefinitionMapType mapType)
    {
        return mapType == ZoneDefinitionMapType::ZM;
    }

    constexpr std::array<const char*, 10> REQUIRED_ZM_WORLDSPAWN_KEYS{
        "lightgridoffset",
        "lutmaterial",
        "fogtime",
        "fsi",
        "wsi",
        "skyboxmodel",
        "newsun",
        "lightingquality",
        "removeredundantlinks",
        "guid",
    };

    [[nodiscard]] bool KeyLooksLikeZBarrierReference(std::string key)
    {
        utils::MakeStringLowerCase(key);
        return key.contains("zbarrier");
    }

    [[nodiscard]] bool TryParseFloat3(const std::string& value, std::array<float, 3>& out)
    {
        std::istringstream stream(value);
        stream >> out[0] >> out[1] >> out[2];
        return !stream.fail();
    }

    [[nodiscard]] bool TryParseFloat(const std::string& value, float& out)
    {
        std::istringstream stream(value);
        stream >> out;
        return !stream.fail();
    }

    [[nodiscard]] bool TryParseInt(const std::string& value, int& out)
    {
        std::istringstream stream(value);
        stream >> out;
        return !stream.fail();
    }

    [[nodiscard]] bool HasEntityKey(const json& entity, const char* key)
    {
        return entity.find(key) != entity.end();
    }

    [[nodiscard]] bool IsBrushModelHelperKey(const std::string& key)
    {
        return key == "box_mins" || key == "box_maxs" || key == "brush_contents" || key == "brush_surfaceflags";
    }

    [[nodiscard]] bool IsTouchVolumeBrushModelEntity(const json& entity)
    {
        const auto classname = entity.find("classname");
        if (classname == entity.end() || !classname->is_string())
            return false;

        const auto value = classname->get<std::string>();
        return value == "info_volume" || value.starts_with("trigger_");
    }

    [[nodiscard]] bool TryGetEntitiesArray(json& root, json*& entities, const ZoneDefinitionMapType mapType)
    {
        if (!root.is_object())
        {
            con::error("T6 {} custom map entities must be defined as a JSON object", GetMapTypeName(mapType));
            return false;
        }

        const auto entitiesIt = root.find("entities");
        if (entitiesIt == root.end())
        {
            con::error("T6 {} custom map entities JSON is missing the required \"entities\" array", GetMapTypeName(mapType));
            return false;
        }

        if (!entitiesIt->is_array())
        {
            con::error("T6 {} custom map entities JSON member \"entities\" must be an array", GetMapTypeName(mapType));
            return false;
        }

        entities = &*entitiesIt;
        return true;
    }

    [[nodiscard]] bool ValidateEntityArray(const json& entities, const ZoneDefinitionMapType mapType)
    {
        if (entities.empty())
        {
            con::error("T6 {} custom map entities JSON must contain at least a worldspawn entity", GetMapTypeName(mapType));
            return false;
        }

        for (auto entityIndex = 0u; entityIndex < entities.size(); entityIndex++)
        {
            const auto& entity = entities[entityIndex];
            if (!entity.is_object())
            {
                con::error("Entity {} in T6 custom map entities JSON must be an object", entityIndex);
                return false;
            }

            const auto classname = entity.find("classname");
            if (classname == entity.end() || !classname->is_string())
            {
                con::error("Entity {} in T6 custom map entities JSON must define a string \"classname\"", entityIndex);
                return false;
            }

            if (entityIndex == 0u && classname->get<std::string>() != "worldspawn")
            {
                con::error("The first entity in a T6 custom map entities JSON file must be \"worldspawn\"");
                return false;
            }

            for (const auto& element : entity.items())
            {
                if (!element.value().is_string())
                {
                    con::error("Entity {} key \"{}\" in T6 custom map entities JSON must be a string value", entityIndex, element.key());
                    return false;
                }
            }
        }

        return true;
    }

    [[nodiscard]] bool ValidateZombieRuntimeEntityKeys(const json& entities)
    {
        const auto& worldspawn = entities[0];
        for (const auto* key : REQUIRED_ZM_WORLDSPAWN_KEYS)
        {
            const auto element = worldspawn.find(key);
            if (element == worldspawn.end() || !element->is_string() || element->get<std::string>().empty())
            {
                con::error("T6 ZM custom map worldspawn requires a non-empty string \"{}\" key", key);
                return false;
            }
        }

        for (auto entityIndex = 0u; entityIndex < entities.size(); entityIndex++)
        {
            const auto guid = entities[entityIndex].find("guid");
            if (guid == entities[entityIndex].end() || !guid->is_string() || guid->get<std::string>().empty())
            {
                con::error("T6 ZM custom map entity {} requires a non-empty string \"guid\" key", entityIndex);
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] bool TryBuildBoxBrushModel(const json& entity, const unsigned entityIndex, map::T6MapBrushModel& brushModel)
    {
        const auto hasMins = HasEntityKey(entity, "box_mins");
        const auto hasMaxs = HasEntityKey(entity, "box_maxs");
        const auto hasContents = HasEntityKey(entity, "brush_contents");
        const auto hasSurfaceFlags = HasEntityKey(entity, "brush_surfaceflags");
        if (!hasMins && !hasMaxs && !hasContents && !hasSurfaceFlags)
            return false;

        if (!hasMins || !hasMaxs)
        {
            con::error("T6 custom map entity {} defines brush model metadata but does not define both \"box_mins\" and \"box_maxs\"", entityIndex);
            throw std::runtime_error("invalid brush model metadata");
        }

        if (HasEntityKey(entity, "model"))
        {
            con::error("T6 custom map entity {} cannot define both an authored \"model\" key and generated brush model bounds", entityIndex);
            throw std::runtime_error("invalid brush model metadata");
        }

        if (IsTouchVolumeBrushModelEntity(entity))
        {
            brushModel.m_contents = GENERATED_TOUCH_VOLUME_CONTENTS;
            brushModel.m_surface_flags = GENERATED_TOUCH_VOLUME_SURFACE_FLAGS;
        }

        if (!TryParseFloat3(entity["box_mins"].get<std::string>(), brushModel.m_mins))
        {
            con::error("T6 custom map entity {} has invalid box_mins \"{}\"", entityIndex, entity["box_mins"].get<std::string>());
            throw std::runtime_error("invalid brush model metadata");
        }

        if (!TryParseFloat3(entity["box_maxs"].get<std::string>(), brushModel.m_maxs))
        {
            con::error("T6 custom map entity {} has invalid box_maxs \"{}\"", entityIndex, entity["box_maxs"].get<std::string>());
            throw std::runtime_error("invalid brush model metadata");
        }

        for (auto axis = 0u; axis < 3u; axis++)
        {
            if (brushModel.m_mins[axis] > brushModel.m_maxs[axis])
            {
                con::error("T6 custom map entity {} has box_mins greater than box_maxs on axis {}", entityIndex, axis);
                throw std::runtime_error("invalid brush model metadata");
            }
        }

        if (hasContents && !TryParseInt(entity["brush_contents"].get<std::string>(), brushModel.m_contents))
        {
            con::error("T6 custom map entity {} has invalid brush_contents \"{}\"", entityIndex, entity["brush_contents"].get<std::string>());
            throw std::runtime_error("invalid brush model metadata");
        }

        if (hasSurfaceFlags && !TryParseInt(entity["brush_surfaceflags"].get<std::string>(), brushModel.m_surface_flags))
        {
            con::error("T6 custom map entity {} has invalid brush_surfaceflags \"{}\"", entityIndex, entity["brush_surfaceflags"].get<std::string>());
            throw std::runtime_error("invalid brush model metadata");
        }

        return true;
    }

    [[nodiscard]] bool BuildEntityStringAndBrushModels(const json& entities,
                                                       std::string& entityString,
                                                       std::vector<map::T6MapBrushModel>& brushModels)
    {
        try
        {
            for (auto entityIndex = 0u; entityIndex < entities.size(); entityIndex++)
            {
                const auto& entity = entities[entityIndex];
                std::optional<map::T6MapBrushModel> brushModel;
                map::T6MapBrushModel parsedBrushModel;
                if (TryBuildBoxBrushModel(entity, entityIndex, parsedBrushModel))
                    brushModel = parsedBrushModel;

                entityString.append("{\n");

                for (const auto& element : entity.items())
                {
                    if (IsBrushModelHelperKey(element.key()))
                        continue;

                    entityString.append(std::format("\"{}\" \"{}\"\n", element.key(), element.value().get<std::string>()));
                }

                if (brushModel)
                {
                    brushModels.emplace_back(*brushModel);
                    entityString.append(std::format("\"model\" \"*{}\"\n", brushModels.size()));
                }

                entityString.append("}\n");
            }
        }
        catch (const std::runtime_error&)
        {
            return false;
        }

        return true;
    }

    void CollectZBarrierDependencies(const json& entities, std::vector<std::string>& dependencies)
    {
        for (const auto& entity : entities)
        {
            for (const auto& element : entity.items())
            {
                if (!KeyLooksLikeZBarrierReference(element.key()))
                    continue;

                const auto assetName = element.value().get<std::string>();
                if (!assetName.empty())
                    dependencies.emplace_back(assetName);
            }
        }
    }

    void AddUniqueDependency(std::vector<std::string>& dependencies, std::string assetName)
    {
        if (assetName.empty())
            return;

        if (std::find(dependencies.begin(), dependencies.end(), assetName) == dependencies.end())
            dependencies.emplace_back(std::move(assetName));
    }

    [[nodiscard]] std::string NormalizeT6AssetName(std::string assetName)
    {
        utils::MakeStringLowerCase(assetName);
        return assetName;
    }

    [[nodiscard]] std::string BuildVisionRawFileName(std::string assetName)
    {
        utils::MakeStringLowerCase(assetName);

        if (!assetName.ends_with(".vision"))
            assetName.append(".vision");

        if (!assetName.starts_with("vision/"))
            assetName.insert(0u, "vision/");

        return assetName;
    }

    void CollectZombieWorldspawnDependencies(const json& entities,
                                             std::vector<std::string>& materialDependencies,
                                             std::vector<std::string>& rawfileDependencies,
                                             std::vector<std::string>& xmodelDependencies)
    {
        const auto& worldspawn = entities[0];

        AddUniqueDependency(materialDependencies, NormalizeT6AssetName(worldspawn["lutmaterial"].get<std::string>()));
        AddUniqueDependency(rawfileDependencies, BuildVisionRawFileName(worldspawn["fsi"].get<std::string>()));
        AddUniqueDependency(rawfileDependencies, BuildVisionRawFileName(worldspawn["wsi"].get<std::string>()));
        AddUniqueDependency(xmodelDependencies, NormalizeT6AssetName(worldspawn["skyboxmodel"].get<std::string>()));
    }

    [[nodiscard]] std::string GetEntityStringValue(const json& entity, const char* key)
    {
        const auto element = entity.find(key);
        if (element == entity.end())
            return {};

        return element->get<std::string>();
    }

    [[nodiscard]] bool TryCollectPathNodes(const json& entities, std::vector<map::T6MapPathNode>& pathNodes)
    {
        for (auto entityIndex = 0u; entityIndex < entities.size(); entityIndex++)
        {
            const auto& entity = entities[entityIndex];
            const auto classname = GetEntityStringValue(entity, "classname");
            if (classname != "node_pathnode")
                continue;

            const auto origin = GetEntityStringValue(entity, "origin");
            if (origin.empty())
            {
                con::error("T6 custom map entity {} is a node_pathnode but does not define an origin", entityIndex);
                return false;
            }

            map::T6MapPathNode pathNode;
            if (!TryParseFloat3(origin, pathNode.m_origin))
            {
                con::error("T6 custom map entity {} has an invalid node_pathnode origin \"{}\"", entityIndex, origin);
                return false;
            }

            const auto angles = GetEntityStringValue(entity, "angles");
            if (!angles.empty())
            {
                std::array<float, 3> parsedAngles{};
                if (!TryParseFloat3(angles, parsedAngles))
                {
                    con::error("T6 custom map entity {} has invalid node_pathnode angles \"{}\"", entityIndex, angles);
                    return false;
                }

                pathNode.m_yaw = parsedAngles[1];
            }

            const auto angle = GetEntityStringValue(entity, "angle");
            if (!angle.empty() && !TryParseFloat(angle, pathNode.m_yaw))
            {
                con::error("T6 custom map entity {} has invalid node_pathnode angle \"{}\"", entityIndex, angle);
                return false;
            }

            const auto spawnFlags = GetEntityStringValue(entity, "spawnflags");
            if (!spawnFlags.empty() && !TryParseInt(spawnFlags, pathNode.m_spawn_flags))
            {
                con::error("T6 custom map entity {} has invalid node_pathnode spawnflags \"{}\"", entityIndex, spawnFlags);
                return false;
            }

            pathNodes.emplace_back(pathNode);
        }

        return true;
    }
} // namespace

namespace map
{
    std::optional<T6MapEntitySource> LoadEntitySourceT6(ISearchPath& searchPath, const ZoneDefinitionMapType mapType)
    {
        try
        {
            const auto entityFile = searchPath.Open(ENTITY_FILE);
            if (!entityFile.IsOpen())
            {
                if (RequiresAuthoredEntities(mapType))
                {
                    con::error("T6 ZM custom maps require an authored entity file at {}", ENTITY_FILE);
                    return std::nullopt;
                }

                T6MapEntitySource defaultSource;
                defaultSource.m_entity_string = DEFAULT_ENTITY_STRING;
                return defaultSource;
            }

            auto root = json::parse(*entityFile.m_stream);
            json* entities = nullptr;
            if (!TryGetEntitiesArray(root, entities, mapType))
                return std::nullopt;

            if (!ValidateEntityArray(*entities, mapType))
                return std::nullopt;
            if (IsZombieMapType(mapType) && !ValidateZombieRuntimeEntityKeys(*entities))
                return std::nullopt;

            T6MapEntitySource result;
            if (!BuildEntityStringAndBrushModels(*entities, result.m_entity_string, result.m_brush_models))
                return std::nullopt;

            if (!TryCollectPathNodes(*entities, result.m_path_nodes))
                return std::nullopt;

            if (IsZombieMapType(mapType))
            {
                CollectZombieWorldspawnDependencies(
                    *entities, result.m_material_dependencies, result.m_rawfile_dependencies, result.m_xmodel_dependencies);
                CollectZBarrierDependencies(*entities, result.m_zbarrier_dependencies);
            }

            return result;
        }
        catch (const json::exception& e)
        {
            con::error("JSON error when parsing T6 custom map entities: {}", e.what());
            return std::nullopt;
        }
    }
} // namespace map
