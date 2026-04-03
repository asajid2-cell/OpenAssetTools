#include "BSPLinker.h"

#include "ClipMapLinker.h"
#include "ComWorldLinker.h"
#include "GameWorldMpLinker.h"
#include "GameWorldSpLinker.h"
#include "GfxWorldLinker.h"
#include "MapEntsLinker.h"
#include "SkinnedVertsLinker.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <format>

using namespace T6;

namespace BSP
{
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

        [[nodiscard]] bool TryLoadScriptDependency(
            const std::string& assetName, ISearchPath& searchPath, AssetCreationContext& context, const bool required)
        {
            if (!required)
            {
                const auto scriptFile = searchPath.Open(assetName);
                if (!scriptFile.IsOpen())
                    return true;
            }

            return context.LoadDependency<AssetScript>(assetName);
        }
    } // namespace

    void BSPLinker::AddEmptyFootstepTableAsset(const std::string& assetName) const
    {
        if (assetName.empty())
            return;

        auto* footstepTable = m_memory.Alloc<FootstepTableDef>();
        footstepTable->name = m_memory.Dup(assetName.c_str());
        memset(footstepTable->sndAliasTable, 0, sizeof(footstepTable->sndAliasTable));

        m_context.AddAsset<AssetFootstepTable>(assetName, footstepTable);
    }

    bool BSPLinker::AddMapScriptDependencies(const BSPData& bsp) const
    {
        if (bsp.mapType == ZoneDefinitionMapType::ZM && !LooksLikeZmMapName(bsp.name))
        {
            con::warn(
                "T6 ZM custom map '{}' does not use a conventional zombie-style name such as 'zm_*'. "
                "Tool-side linking will continue, but zombie naming conventions are still used elsewhere in T6.",
                bsp.name);
        }

        // T6 map scripts are conventionally looked up from maps/mp and
        // clientscripts/mp, including ZM and campaign map names such as zm_* and mp_drone.
        const std::array scriptAssets{
            std::format("maps/mp/{}.gsc", bsp.name),
            std::format("maps/mp/{}_amb.gsc", bsp.name),
            std::format("maps/mp/{}_fx.gsc", bsp.name),
            std::format("clientscripts/mp/{}.csc", bsp.name),
            std::format("clientscripts/mp/{}_amb.csc", bsp.name),
            std::format("clientscripts/mp/{}_fx.csc", bsp.name),
        };

        if (ShouldRequireConventionalMapScripts(bsp.mapType))
        {
            for (const auto& scriptAsset : scriptAssets)
            {
                if (!TryLoadScriptDependency(scriptAsset, m_search_path, m_context, true))
                {
                    con::error("T6 {} custom maps require the conventional map script dependency \"{}\"",
                               bsp.mapType == ZoneDefinitionMapType::ZM ? "ZM" : "MP",
                               scriptAsset);
                    return false;
                }
            }

            return true;
        }

        auto loadedScriptCount = 0u;
        for (const auto& scriptAsset : scriptAssets)
        {
            const auto scriptFile = m_search_path.Open(scriptAsset);
            if (!scriptFile.IsOpen())
                continue;

            if (!m_context.LoadDependency<AssetScript>(scriptAsset))
                return false;

            loadedScriptCount++;
        }

        if (loadedScriptCount == 0u)
        {
            con::warn(
                "No conventional map scripts were found for '{}' under maps/mp or clientscripts/mp. "
                "Provide scripts explicitly in the zone definition if the target mode needs them.",
                bsp.name);
        }

        return true;
    }

    bool BSPLinker::AddDefaultRequiredAssets(const BSPData& bsp) const
    {
        if (!AddMapScriptDependencies(bsp))
            return false;

        AddEmptyFootstepTableAsset("default_1st_person");
        AddEmptyFootstepTableAsset("default_3rd_person");
        AddEmptyFootstepTableAsset("default_1st_person_quiet");
        AddEmptyFootstepTableAsset("default_3rd_person_quiet");
        AddEmptyFootstepTableAsset("default_3rd_person_loud");
        AddEmptyFootstepTableAsset("default_ai");

        if (!m_context.LoadDependency<AssetRawFile>("animtrees/fxanim_props.atr"))
            return false;

        return true;
    }

    BSPLinker::BSPLinker(MemoryManager& memory, ISearchPath& searchPath, AssetCreationContext& context)
        : m_memory(memory),
          m_search_path(searchPath),
          m_context(context)
    {
    }

    bool BSPLinker::LinkBSP(const BSPData& bsp) const
    {
        const auto debugStage = [](const char* stage)
        {
            std::fprintf(stderr, "[oat][bsplink] %s\n", stage);
            std::fflush(stderr);
        };

        debugStage("AddDefaultRequiredAssets");
        if (!AddDefaultRequiredAssets(bsp))
            return false;

        ComWorldLinker comWorldLinker(m_memory, m_search_path, m_context);
        ClipMapLinker clipMapLinker(m_memory, m_search_path, m_context);
        GameWorldMpLinker gameWorldMpLinker(m_memory, m_search_path, m_context);
        GameWorldSpLinker gameWorldSpLinker(m_memory, m_search_path, m_context);
        GfxWorldLinker gfxWorldLinker(m_memory, m_search_path, m_context);
        MapEntsLinker mapEntsLinker(m_memory, m_search_path, m_context);
        SkinnedVertsLinker skinnedVertsLinker(m_memory, m_search_path, m_context);

        debugStage("LinkComWorld");
        auto* comWorld = comWorldLinker.LinkComWorld(bsp);
        if (!comWorld)
            return false;
        m_context.AddAsset<AssetComWorld>(comWorld->name, comWorld);

        debugStage("LinkMapEnts");
        auto* mapEnts = mapEntsLinker.LinkMapEnts(bsp);
        if (!mapEnts)
            return false;
        m_context.AddAsset<AssetMapEnts>(mapEnts->name, mapEnts);

        if (bsp.mapType == ZoneDefinitionMapType::MP)
        {
            debugStage("LinkGameWorldMp");
            auto* gameWorldMp = gameWorldMpLinker.LinkGameWorldMp(bsp);
            if (!gameWorldMp)
                return false;
            m_context.AddAsset<AssetGameWorldMp>(gameWorldMp->name, gameWorldMp);
        }
        else
        {
            debugStage("LinkGameWorldSp");
            auto* gameWorldSp = gameWorldSpLinker.LinkGameWorldSp(bsp);
            if (!gameWorldSp)
                return false;
            m_context.AddAsset<AssetGameWorldSp>(gameWorldSp->name, gameWorldSp);
        }

        debugStage("LinkSkinnedVerts");
        auto* skinnedVerts = skinnedVertsLinker.LinkSkinnedVerts(bsp);
        if (!skinnedVerts)
            return false;
        m_context.AddAsset<AssetSkinnedVerts>(skinnedVerts->name, skinnedVerts);

        debugStage("LinkGfxWorld");
        auto* gfxWorld = gfxWorldLinker.LinkGfxWorld(bsp); // requires mapents asset
        if (!gfxWorld)
            return false;
        m_context.AddAsset<AssetGfxWorld>(gfxWorld->name, gfxWorld);

        debugStage("LinkClipMap");
        auto* clipMap = clipMapLinker.LinkClipMap(bsp); // requires gfxworld and mapents asset
        if (!clipMap)
            return false;
        m_context.AddAsset<AssetClipMap>(clipMap->name, clipMap);
        m_context.AddAsset<AssetClipMapPvs>(clipMap->name, clipMap);

        debugStage("done");
        return true;
    }
} // namespace BSP
