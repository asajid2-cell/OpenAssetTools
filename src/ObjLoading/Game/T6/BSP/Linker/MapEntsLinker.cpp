#include "MapEntsLinker.h"

#include "Game/T6/BSP/BSPUtil.h"
#include "Utils/StringUtils.h"

#include <nlohmann/json.hpp>

using namespace nlohmann;
using namespace T6;

namespace
{
    [[nodiscard]] const char* GetDefaultMapEntsString(const ZoneDefinitionMapType mapType)
    {
        return mapType == ZoneDefinitionMapType::MP ? BSP::BSPLinkingConstants::DEFAULT_MAP_ENTS_STRING
                                                    : BSP::BSPLinkingConstants::DEFAULT_MAP_ENTS_STRING_NON_MP;
    }

    [[nodiscard]] bool ShouldAddMpSpawnPoints(const ZoneDefinitionMapType mapType)
    {
        return mapType == ZoneDefinitionMapType::MP;
    }

    [[nodiscard]] bool ShouldRequireEntityFile(const ZoneDefinitionMapType mapType)
    {
        return mapType == ZoneDefinitionMapType::ZM;
    }

    [[nodiscard]] bool IsZombieMapType(const ZoneDefinitionMapType mapType)
    {
        return mapType == ZoneDefinitionMapType::ZM;
    }

    [[nodiscard]] bool KeyLooksLikeZBarrierReference(std::string key)
    {
        utils::MakeStringLowerCase(key);
        return key.contains("zbarrier");
    }

    [[nodiscard]] bool TryGetEntitiesArray(json& entJs, json*& entities, const ZoneDefinitionMapType mapType)
    {
        if (!entJs.is_object())
        {
            con::error("T6 {} custom map entities must be defined as a JSON object", mapType == ZoneDefinitionMapType::ZM ? "ZM" : "map");
            return false;
        }

        const auto entitiesIt = entJs.find("entities");
        if (entitiesIt == entJs.end())
        {
            con::error("T6 {} custom map entities JSON is missing the required \"entities\" array",
                       mapType == ZoneDefinitionMapType::ZM ? "ZM" : "map");
            return false;
        }

        if (!entitiesIt->is_array())
        {
            con::error("T6 {} custom map entities JSON member \"entities\" must be an array",
                       mapType == ZoneDefinitionMapType::ZM ? "ZM" : "map");
            return false;
        }

        entities = &*entitiesIt;
        return true;
    }

    [[nodiscard]] bool ValidateEntityArray(const json& entArrayJs, const ZoneDefinitionMapType mapType)
    {
        if (entArrayJs.empty())
        {
            con::error("T6 {} custom map entities JSON must contain at least a worldspawn entity",
                       mapType == ZoneDefinitionMapType::ZM ? "ZM" : "map");
            return false;
        }

        for (size_t entIdx = 0; entIdx < entArrayJs.size(); entIdx++)
        {
            const auto& entity = entArrayJs[entIdx];
            if (!entity.is_object())
            {
                con::error("Entity {} in T6 custom map entities JSON must be an object", entIdx);
                return false;
            }

            const auto classnameIt = entity.find("classname");
            if (classnameIt == entity.end() || !classnameIt->is_string())
            {
                con::error("Entity {} in T6 custom map entities JSON must define a string \"classname\"", entIdx);
                return false;
            }

            if (entIdx == 0 && classnameIt->get<std::string>() != "worldspawn")
            {
                con::error("The first entity in a T6 custom map entities JSON file must be \"worldspawn\"");
                return false;
            }

            for (const auto& element : entity.items())
            {
                if (!element.value().is_string())
                {
                    con::error("Entity {} key \"{}\" in T6 custom map entities JSON must be a string value", entIdx, element.key());
                    return false;
                }
            }
        }

        return true;
    }

    bool LoadZombieEntityDependencies(const json& entArrayJs, AssetCreationContext& context)
    {
        for (const auto& entity : entArrayJs)
        {
            if (!entity.is_object())
                continue;

            for (const auto& element : entity.items())
            {
                if (!KeyLooksLikeZBarrierReference(element.key()) || !element.value().is_string())
                    continue;

                const auto assetName = element.value().get<std::string>();
                if (assetName.empty())
                    continue;

                if (!context.LoadDependency<AssetZBarrier>(assetName))
                {
                    con::error("Failed to load zbarrier dependency \"{}\" referenced by map entity key \"{}\"", assetName, element.key());
                    return false;
                }
            }
        }

        return true;
    }

    bool parseMapEntsJSON(json& entArrayJs, std::string& entityString)
    {
        for (size_t entIdx = 0; entIdx < entArrayJs.size(); entIdx++)
        {
            auto& entity = entArrayJs[entIdx];

            if (entIdx == 0)
            {
                std::string className;
                entity.at("classname").get_to(className);
                if (className != "worldspawn")
                {
                    con::error("ERROR: first entity in the map entity string must be the worldspawn class!");
                    return false;
                }
            }

            entityString.append("{\n");

            for (auto& element : entity.items())
            {
                std::string key = element.key();
                std::string value = element.value();
                entityString.append(std::format("\"{}\" \"{}\"\n", key, value));
            }

            entityString.append("}\n");
        }

        return true;
    }

    void parseSpawnpointJSON(json& entArrayJs, std::string& entityString, const char* spawnpointNames[], size_t nameCount)
    {
        for (auto& element : entArrayJs.items())
        {
            std::string origin;
            std::string angles;
            auto& entity = element.value();
            entity.at("origin").get_to(origin);
            entity.at("angles").get_to(angles);

            for (size_t nameIdx = 0; nameIdx < nameCount; nameIdx++)
            {
                entityString.append("{\n");
                entityString.append(std::format("\"origin\" \"{}\"\n", origin));
                entityString.append(std::format("\"angles\" \"{}\"\n", angles));
                entityString.append(std::format("\"classname\" \"{}\"\n", spawnpointNames[nameIdx]));
                entityString.append("}\n");
            }
        }
    }
} // namespace

namespace BSP
{
    MapEntsLinker::MapEntsLinker(MemoryManager& memory, ISearchPath& searchPath, AssetCreationContext& context)
        : m_memory(memory),
          m_search_path(searchPath),
          m_context(context)
    {
    }

    MapEnts* MapEntsLinker::LinkMapEnts(const BSPData& bsp) const
    {
        try
        {
            json entJs;
            json* entities = nullptr;
            const auto entityFilePath = GetFileNameForBSPAsset("entities.json");
            const auto entFile = m_search_path.Open(entityFilePath);
            if (!entFile.IsOpen())
            {
                if (ShouldRequireEntityFile(bsp.mapType))
                {
                    con::error("T6 ZM custom maps require an authored entity file at {}", entityFilePath);
                    return nullptr;
                }

                con::warn("Can't find entity file {}, using default entities instead", entityFilePath);
                entJs = json::parse(GetDefaultMapEntsString(bsp.mapType));
            }
            else
            {
                entJs = json::parse(*entFile.m_stream);
            }

            if (!TryGetEntitiesArray(entJs, entities, bsp.mapType))
                return nullptr;

            if (!ValidateEntityArray(*entities, bsp.mapType))
                return nullptr;

            if (IsZombieMapType(bsp.mapType) && !LoadZombieEntityDependencies(*entities, m_context))
                return nullptr;

            std::string entityString;
            if (!parseMapEntsJSON(*entities, entityString))
                return nullptr;

            if (ShouldAddMpSpawnPoints(bsp.mapType))
            {
                json spawnJs;
                const auto spawnFilePath = GetFileNameForBSPAsset("spawns.json");
                const auto spawnFile = m_search_path.Open(spawnFilePath);
                if (!spawnFile.IsOpen())
                {
                    con::warn("Cant find spawn file {}, setting spawns to 0 0 0", spawnFilePath);
                    spawnJs = json::parse(BSPLinkingConstants::DEFAULT_SPAWN_POINT_STRING);
                }
                else
                {
                    spawnJs = json::parse(*spawnFile.m_stream);
                }

                constexpr auto defenderNameCount = std::extent_v<decltype(BSPGameConstants::DEFENDER_SPAWN_POINT_NAMES)>;
                constexpr auto attackerNameCount = std::extent_v<decltype(BSPGameConstants::ATTACKER_SPAWN_POINT_NAMES)>;
                constexpr auto ffaNameCount = std::extent_v<decltype(BSPGameConstants::FFA_SPAWN_POINT_NAMES)>;

                parseSpawnpointJSON(spawnJs["attackers"], entityString, BSPGameConstants::ATTACKER_SPAWN_POINT_NAMES, attackerNameCount);
                parseSpawnpointJSON(spawnJs["defenders"], entityString, BSPGameConstants::DEFENDER_SPAWN_POINT_NAMES, defenderNameCount);
                parseSpawnpointJSON(spawnJs["FFA"], entityString, BSPGameConstants::FFA_SPAWN_POINT_NAMES, ffaNameCount);
            }

            MapEnts* mapEnts = m_memory.Alloc<MapEnts>();
            mapEnts->name = m_memory.Dup(bsp.bspName.c_str());

            mapEnts->entityString = m_memory.Dup(entityString.c_str());
            mapEnts->numEntityChars = static_cast<int>(entityString.length() + 1); // numEntityChars includes the null character

            // don't need these
            mapEnts->trigger.count = 0;
            mapEnts->trigger.models = nullptr;
            mapEnts->trigger.hullCount = 0;
            mapEnts->trigger.hulls = nullptr;
            mapEnts->trigger.slabCount = 0;
            mapEnts->trigger.slabs = nullptr;

            return mapEnts;
        }
        catch (const json::exception& e)
        {
            con::error("JSON error when parsing map ents and spawns: {}", e.what());
            return nullptr;
        }
    }
} // namespace BSP
