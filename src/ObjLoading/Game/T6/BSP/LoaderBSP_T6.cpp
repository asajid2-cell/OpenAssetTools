#include "LoaderBSP_T6.h"

#include "BSPCreator.h"
#include "BSPUtil.h"
#include "Linker/BSPLinker.h"

using namespace BSP;

namespace
{
    class BSPLoader final : public IAssetCreator
    {
    public:
        BSPLoader(MemoryManager& memory, ISearchPath& searchPath, Zone& zone, const ZoneDefinitionMapType mapType)
            : m_memory(memory),
              m_search_path(searchPath),
              m_zone(zone),
              m_map_type(mapType)
        {
        }

        [[nodiscard]] std::optional<asset_type_t> GetHandlingAssetType() const override
        {
            // don't handle any asset types
            return std::nullopt;
        }

        AssetCreationResult CreateAsset(const std::string& assetName, AssetCreationContext& context) override
        {
            // BSP assets are added in the finalize zone step
            return AssetCreationResult::NoAction();
        }

        void FinalizeZone(AssetCreationContext& context) override
        {
            const auto bsp = CreateBSPData(m_zone.m_name, m_search_path, m_map_type);
            if (!bsp)
            {
                context.ReportFailure();
                return;
            }

            BSPLinker linker(m_memory, m_search_path, context);
            const auto result = linker.LinkBSP(*bsp);
            if (!result)
            {
                con::error("BSP link has failed.");
                context.ReportFailure();
            }
        }

    private:
        MemoryManager& m_memory;
        ISearchPath& m_search_path;
        Zone& m_zone;
        ZoneDefinitionMapType m_map_type;
    };
} // namespace

namespace BSP
{
    std::unique_ptr<IAssetCreator> CreateLoaderT6(
        MemoryManager& memory, ISearchPath& searchPath, Zone& zone, const ZoneDefinitionMapType mapType)
    {
        return std::make_unique<BSPLoader>(memory, searchPath, zone, mapType);
    }
} // namespace BSP
