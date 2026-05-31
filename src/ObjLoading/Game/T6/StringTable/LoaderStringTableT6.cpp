#include "LoaderStringTableT6.h"

#include "Csv/CsvStream.h"
#include "Game/T6/CommonT6.h"
#include "Game/T6/T6.h"
#include "StringTable/StringTableLoader.h"
#include "Utils/Logging/Log.h"

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_set>

using namespace T6;

namespace
{
    [[nodiscard]] bool IsMapConfigStringTableName(const std::string& assetName)
    {
        constexpr std::string_view PREFIX = "mp/configstrings/configstrings_";
        constexpr std::string_view SUFFIX = ".csv";

        return assetName.size() > PREFIX.size() + SUFFIX.size()
               && assetName.compare(0u, PREFIX.size(), PREFIX.data(), PREFIX.size()) == 0
               && assetName.compare(assetName.size() - SUFFIX.size(), SUFFIX.size(), SUFFIX.data(), SUFFIX.size()) == 0;
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
        const auto hadAsset = context.HasAsset<AssetXModel>(assetName);
        const auto* asset = context.LoadDependencyGeneric(AssetXModel::EnumEntry, assetName, false);
        if (asset && !hadAsset)
            loadedCount++;

        return asset != nullptr;
    }

    void AddConfigStringXModelDependencies(const std::string& assetName, const StringTable* stringTable, AssetCreationContext& context)
    {
        if (!IsMapConfigStringTableName(assetName) || !stringTable || !stringTable->values)
            return;

        context.LoadDependencyGeneric(AssetSkinnedVerts::EnumEntry, "skinnedverts", false);

        auto loadedCount = 0u;
        std::unordered_set<std::string> seenCandidates;
        const auto cellCount = stringTable->columnCount * stringTable->rowCount;
        for (auto cellIndex = 0; cellIndex < cellCount; cellIndex++)
        {
            const auto* value = stringTable->values[cellIndex].string;
            if (!value)
                continue;

            auto candidate = NormalizeConfigStringToken(value);
            if (!LooksLikeConfigStringXModelName(candidate) || !seenCandidates.emplace(candidate).second)
                continue;

            TryLoadConfigStringXModelDependency(candidate, context, loadedCount);

            if (!candidate.empty() && candidate.front() != ',')
                TryLoadConfigStringXModelDependency("," + candidate, context, loadedCount);
        }

        if (loadedCount > 0u)
            con::info("Loaded {} T6 xmodel dependencies referenced by {}", loadedCount, assetName);
    }

    class StringTableLoader final : public AssetCreator<AssetStringTable>
    {
    public:
        StringTableLoader(MemoryManager& memory, ISearchPath& searchPath)
            : m_memory(memory),
              m_search_path(searchPath)
        {
        }

        AssetCreationResult CreateAsset(const std::string& assetName, AssetCreationContext& context) override
        {
            const auto file = m_search_path.Open(assetName);
            if (!file.IsOpen())
                return AssetCreationResult::NoAction();

            string_table::StringTableLoaderV3<StringTable, Common::Com_HashString> loader;
            auto* stringTable = loader.LoadFromStream(assetName, m_memory, *file.m_stream);

            AddConfigStringXModelDependencies(assetName, stringTable, context);

            return AssetCreationResult::Success(context.AddAsset<AssetStringTable>(assetName, stringTable));
        }

    private:
        MemoryManager& m_memory;
        ISearchPath& m_search_path;
    };
} // namespace

namespace string_table
{
    std::unique_ptr<AssetCreator<AssetStringTable>> CreateLoaderT6(MemoryManager& memory, ISearchPath& searchPath)
    {
        return std::make_unique<StringTableLoader>(memory, searchPath);
    }
} // namespace string_table
