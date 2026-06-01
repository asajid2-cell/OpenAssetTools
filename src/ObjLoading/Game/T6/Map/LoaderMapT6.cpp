#include "LoaderMapT6.h"

#include "Game/T6/T6.h"
#include "MapClipMapT6.h"
#include "MapGeometryT6.h"
#include "MapGfxWorldT6.h"
#include "MapSourceT6.h"
#include "MapWorldT6.h"
#include "Utils/Logging/Log.h"

#include <array>
#include <cctype>
#include <cstring>
#include <format>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace
{
    [[nodiscard]] bool LooksLikeZmMapName(const std::string& mapName)
    {
        return mapName.starts_with("zm_") || mapName.ends_with("_zm");
    }

    [[nodiscard]] bool ShouldRequireConventionalMapScripts(const ZoneDefinitionMapType mapType)
    {
        return mapType == ZoneDefinitionMapType::MP || mapType == ZoneDefinitionMapType::ZM;
    }

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
            return "none";
        }
    }

    [[nodiscard]] bool TryLoadScriptDependency(const std::string& assetName,
                                               ISearchPath& searchPath,
                                               AssetCreationContext& context,
                                               const bool required)
    {
        if (!required)
        {
            const auto scriptFile = searchPath.Open(assetName);
            if (!scriptFile.IsOpen())
                return true;
        }

        return context.LoadDependency<T6::AssetScript>(assetName) != nullptr;
    }

    void AddEmptyFootstepTableAsset(MemoryManager& memory, AssetCreationContext& context, const std::string& assetName)
    {
        auto* footstepTable = memory.Alloc<T6::FootstepTableDef>();
        footstepTable->name = memory.Dup(assetName.c_str());
        std::memset(footstepTable->sndAliasTable, 0, sizeof(footstepTable->sndAliasTable));

        context.AddAsset<T6::AssetFootstepTable>(assetName, footstepTable);
    }

    void AddEmptyMapMarkerRawFile(MemoryManager& memory, AssetCreationContext& context, const std::string& mapName)
    {
        if (context.HasAsset<T6::AssetRawFile>(mapName))
            return;

        auto* rawFile = memory.Alloc<T6::RawFile>();
        rawFile->name = memory.Dup(mapName.c_str());
        rawFile->len = 0;
        auto* buffer = memory.Alloc<char>(1u);
        buffer[0] = '\0';
        rawFile->buffer = static_cast<T6::char16*>(buffer);

        context.AddAsset<T6::AssetRawFile>(mapName, rawFile);
    }

    [[nodiscard]] std::string_view TrimAscii(std::string_view value)
    {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
            value.remove_prefix(1u);

        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
            value.remove_suffix(1u);

        return value;
    }

    [[nodiscard]] std::string NormalizeConfigStringToken(std::string_view token)
    {
        token = TrimAscii(token);

        while (token.size() >= 2u && token.front() == '"' && token.back() == '"')
        {
            token.remove_prefix(1u);
            token.remove_suffix(1u);
            token = TrimAscii(token);
        }

        return std::string(token);
    }

    [[nodiscard]] bool LooksLikeConfigStringXModelName(const std::string& value)
    {
        if (value.empty() || value.size() > 128u)
            return false;

        auto hasAlpha = false;
        for (const auto c : value)
        {
            const auto uc = static_cast<unsigned char>(c);
            if (std::isspace(uc) || c == '\\' || c == '"' || c == ';')
                return false;

            hasAlpha = hasAlpha || std::isalpha(uc) || c == '_';
        }

        return hasAlpha;
    }

    bool TryLoadConfigStringXModelDependency(const std::string& assetName, AssetCreationContext& context, unsigned& loadedCount)
    {
        const auto hadAsset = context.HasAsset<T6::AssetXModel>(assetName);
        const auto* asset = context.LoadDependencyGeneric(T6::AssetXModel::EnumEntry, assetName, false);
        if (asset && !hadAsset)
            loadedCount++;

        return asset != nullptr;
    }

    bool AddConfigStringXModelDependencies(const std::string& mapName, ISearchPath& searchPath, AssetCreationContext& context)
    {
        const auto configStringsFileName = std::format("mp/configstrings/configstrings_{}.csv", mapName);
        const auto configStringsFile = searchPath.Open(configStringsFileName);
        if (!configStringsFile.IsOpen())
            return true;

        std::unordered_set<std::string> candidates;
        std::string line;
        while (std::getline(*configStringsFile.m_stream, line))
        {
            auto candidate = NormalizeConfigStringToken(line);
            if (LooksLikeConfigStringXModelName(candidate))
                candidates.emplace(std::move(candidate));
        }

        auto loadedCount = 0u;
        for (const auto& candidate : candidates)
        {
            TryLoadConfigStringXModelDependency(candidate, context, loadedCount);

            if (!candidate.empty() && candidate.front() != ',')
                TryLoadConfigStringXModelDependency(std::format(",{}", candidate), context, loadedCount);
        }

        if (loadedCount > 0u)
        {
            con::info("Loaded {} T6 xmodel dependencies referenced by {}", loadedCount, configStringsFileName);
        }

        return true;
    }

    [[nodiscard]] bool AddZombieMapVisionDependency(const std::string& mapName, AssetCreationContext& context)
    {
        const auto visionAssetName = std::format("vision/{}.vision", mapName);
        if (context.LoadDependency<T6::AssetRawFile>(visionAssetName))
            return true;

        con::error(
            "T6 ZM custom map \"{}\" requires map vision rawfile \"{}\". "
            "Zombie map scripts set level.script to the map name and load this visionset at runtime.",
            mapName,
            visionAssetName);
        return false;
    }

    [[nodiscard]] bool AddMapScriptDependencies(const std::string& mapName,
                                                const ZoneDefinitionMapType mapType,
                                                ISearchPath& searchPath,
                                                AssetCreationContext& context)
    {
        if (mapType == ZoneDefinitionMapType::ZM && !LooksLikeZmMapName(mapName))
        {
            con::warn(
                "T6 ZM custom map '{}' does not use a conventional zombie-style name such as 'zm_*'. "
                "Tool-side linking will continue, but zombie naming conventions are still used elsewhere in T6.",
                mapName);
        }

        const std::array scriptAssets{
            std::format("maps/mp/{}.gsc", mapName),
            std::format("maps/mp/{}_amb.gsc", mapName),
            std::format("maps/mp/{}_fx.gsc", mapName),
            std::format("clientscripts/mp/{}.csc", mapName),
            std::format("clientscripts/mp/{}_amb.csc", mapName),
            std::format("clientscripts/mp/{}_fx.csc", mapName),
        };

        const auto required = ShouldRequireConventionalMapScripts(mapType);
        auto loadedScriptCount = 0u;

        for (const auto& scriptAsset : scriptAssets)
        {
            if (!TryLoadScriptDependency(scriptAsset, searchPath, context, required))
            {
                if (required)
                    con::error("T6 {} custom maps require the conventional map script dependency \"{}\"",
                               GetMapTypeName(mapType),
                               scriptAsset);

                return false;
            }

            if (!required && searchPath.Open(scriptAsset).IsOpen())
                loadedScriptCount++;
        }

        if (!required && loadedScriptCount == 0u)
        {
            con::warn(
                "No conventional map scripts were found for '{}' under maps/mp or clientscripts/mp. "
                "Provide scripts explicitly in the zone definition if the target mode needs them.",
                mapName);
        }

        return true;
    }

    [[nodiscard]] bool AddDefaultRequiredAssets(MemoryManager& memory,
                                                const std::string& mapName,
                                                const ZoneDefinitionMapType mapType,
                                                ISearchPath& searchPath,
                                                AssetCreationContext& context)
    {
        if (!AddMapScriptDependencies(mapName, mapType, searchPath, context))
            return false;

        if (!AddConfigStringXModelDependencies(mapName, searchPath, context))
            return false;

        if (mapType == ZoneDefinitionMapType::ZM && !AddZombieMapVisionDependency(mapName, context))
            return false;

        AddEmptyMapMarkerRawFile(memory, context, mapName);

        AddEmptyFootstepTableAsset(memory, context, "default_1st_person");
        AddEmptyFootstepTableAsset(memory, context, "default_3rd_person");
        AddEmptyFootstepTableAsset(memory, context, "default_1st_person_quiet");
        AddEmptyFootstepTableAsset(memory, context, "default_3rd_person_quiet");
        AddEmptyFootstepTableAsset(memory, context, "default_3rd_person_loud");
        AddEmptyFootstepTableAsset(memory, context, "default_ai");

        return context.LoadDependency<T6::AssetRawFile>("animtrees/fxanim_props.atr") != nullptr;
    }

    [[nodiscard]] bool AddZombieEntityDependencies(const map::T6MapEntitySource& entitySource, AssetCreationContext& context)
    {
        for (const auto& materialDependency : entitySource.m_material_dependencies)
        {
            if (!context.LoadDependency<T6::AssetMaterial>(materialDependency))
            {
                con::error("Failed to load material dependency \"{}\" referenced by T6 ZM custom map worldspawn", materialDependency);
                return false;
            }
        }

        for (const auto& rawfileDependency : entitySource.m_rawfile_dependencies)
        {
            if (!context.LoadDependency<T6::AssetRawFile>(rawfileDependency))
            {
                con::error("Failed to load rawfile dependency \"{}\" referenced by T6 ZM custom map worldspawn", rawfileDependency);
                return false;
            }
        }

        for (const auto& xmodelDependency : entitySource.m_xmodel_dependencies)
        {
            if (!context.LoadDependency<T6::AssetXModel>(xmodelDependency))
            {
                con::error("Failed to load xmodel dependency \"{}\" referenced by T6 ZM custom map worldspawn", xmodelDependency);
                return false;
            }
        }

        for (const auto& zbarrierDependency : entitySource.m_zbarrier_dependencies)
        {
            if (!context.LoadDependency<T6::AssetZBarrier>(zbarrierDependency))
            {
                con::error("Failed to load zbarrier dependency \"{}\" referenced by T6 ZM custom map entities", zbarrierDependency);
                return false;
            }
        }

        return true;
    }

    class MapLoader final : public IAssetCreator
    {
    public:
        MapLoader(ISearchPath& searchPath, Zone& zone, const ZoneDefinitionMapType mapType, std::string mapAssetName)
            : m_search_path(searchPath),
              m_zone(zone),
              m_map_type(mapType),
              m_map_asset_name(std::move(mapAssetName))
        {
            if (m_map_asset_name.empty())
                m_map_asset_name = m_zone.m_name;
        }

        [[nodiscard]] std::optional<asset_type_t> GetHandlingAssetType() const override
        {
            return std::nullopt;
        }

        AssetCreationResult CreateAsset(const std::string& assetName, AssetCreationContext& context) override
        {
            (void)assetName;
            (void)context;
            return AssetCreationResult::NoAction();
        }

        void FinalizeZone(AssetCreationContext& context) override
        {
            if (m_map_type == ZoneDefinitionMapType::NONE)
                return;

            if (!ValidateSourceFiles())
            {
                context.ReportFailure();
                return;
            }

            const auto entitySource = map::LoadEntitySourceT6(m_search_path, m_map_type);
            if (!entitySource)
            {
                context.ReportFailure();
                return;
            }

            if (m_map_type == ZoneDefinitionMapType::ZM && !AddZombieEntityDependencies(*entitySource, context))
            {
                context.ReportFailure();
                return;
            }

            auto& memory = m_zone.Memory();
            if (!AddDefaultRequiredAssets(memory, m_map_asset_name, m_map_type, m_search_path, context))
            {
                context.ReportFailure();
                return;
            }

            const auto geometry = map::LoadMapGeometryT6(m_search_path, m_map_asset_name, m_map_type);
            if (!geometry)
            {
                context.ReportFailure();
                return;
            }

            if (!map::EmitPreGfxWorldAssetsT6(context, m_zone, m_map_asset_name))
            {
                context.ReportFailure();
                return;
            }

            auto* gfxWorld = map::CreateGfxWorldT6(memory, m_search_path, context, *geometry, *entitySource);
            if (!gfxWorld)
            {
                context.ReportFailure();
                return;
            }

            context.AddAsset<T6::AssetGfxWorld>(geometry->m_world_asset_name, gfxWorld);

            if (!map::EmitRuntimeWorldAssetsT6(context, m_zone, m_map_asset_name, *entitySource, m_map_type))
            {
                context.ReportFailure();
                return;
            }

            auto* clipMap = map::CreateClipMapT6(memory, context, *geometry, *gfxWorld, *entitySource);
            if (!clipMap)
            {
                context.ReportFailure();
                return;
            }

            context.AddAsset<T6::AssetClipMapPvs>(geometry->m_world_asset_name, clipMap);
        }

    private:
        [[nodiscard]] bool RequiredFileExists(const std::string& fileName) const
        {
            auto file = m_search_path.Open(fileName);
            if (file.IsOpen())
                return true;

            con::error("T6 {} custom map \"{}\" requires source file \"{}\"", GetMapTypeName(m_map_type), m_zone.m_name, fileName);
            return false;
        }

        [[nodiscard]] bool ValidateSourceFiles() const
        {
            auto result = true;

            result = RequiredFileExists("bsp/map_gfx.fbx") && result;

            if (m_map_type == ZoneDefinitionMapType::ZM)
                result = RequiredFileExists("bsp/entities.json") && result;

            return result;
        }

        ISearchPath& m_search_path;
        Zone& m_zone;
        ZoneDefinitionMapType m_map_type;
        std::string m_map_asset_name;
    };
} // namespace

namespace map
{
    std::unique_ptr<IAssetCreator> CreateLoaderT6(ISearchPath& searchPath, Zone& zone, const ZoneDefinitionMapType mapType, std::string mapAssetName)
    {
        return std::make_unique<MapLoader>(searchPath, zone, mapType, std::move(mapAssetName));
    }
} // namespace map
