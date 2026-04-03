#include "Linker.h"
#include "OatTestPaths.h"
#include "SystemTestsPaths.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    bool RunLinkerForZone(const fs::path& sourceSearchPath, const std::vector<fs::path>& assetSearchPaths, const fs::path& outputPath, const char* zoneName)
    {
        REQUIRE(!assetSearchPaths.empty());

        std::vector<std::string> arguments{
            "SystemTests",
            "--verbose",
            "--asset-search-path",
            assetSearchPaths.front().string(),
        };

        for (size_t i = 1; i < assetSearchPaths.size(); i++)
        {
            arguments.emplace_back("--add-asset-search-path");
            arguments.emplace_back(assetSearchPaths[i].string());
        }

        arguments.emplace_back("--source-search-path");
        arguments.emplace_back(sourceSearchPath.string());
        arguments.emplace_back("--output-folder");
        arguments.emplace_back(outputPath.string());
        arguments.emplace_back(zoneName);

        std::vector<const char*> argStrings;
        argStrings.reserve(arguments.size());
        for (const auto& argument : arguments)
            argStrings.push_back(argument.c_str());

        LinkerArgs args;

        bool shouldContinue = true;
        const auto couldParseArgs = args.ParseArgs(static_cast<int>(argStrings.size()), argStrings.data(), shouldContinue);

        REQUIRE(couldParseArgs);
        REQUIRE(shouldContinue);

        const auto linker = Linker::Create(std::move(args));
        return linker->Start();
    }

    TEST_CASE("T6 zombies custom map requires conventional scripts", "[t6][system][zm][map]")
    {
        const auto testDir = oat::paths::GetSystemTestsDirectory() / "Game/T6/ZmCustomMapRequirements";
        const auto outputPath = oat::paths::GetTempDirectory("ZmCustomMapMissingScriptsT6");

        const auto linkerResult = RunLinkerForZone(
            testDir, {testDir / "ZmMissingScriptsT6", testDir / "CommonAssets"}, outputPath, "ZmMissingScriptsT6");

        REQUIRE_FALSE(linkerResult);
    }

    TEST_CASE("T6 zombies custom map requires authored entities", "[t6][system][zm][map]")
    {
        const auto testDir = oat::paths::GetSystemTestsDirectory() / "Game/T6/ZmCustomMapRequirements";
        const auto outputPath = oat::paths::GetTempDirectory("ZmCustomMapMissingEntitiesT6");

        const auto linkerResult = RunLinkerForZone(
            testDir, {testDir / "ZmMissingEntitiesT6", testDir / "CommonAssets"}, outputPath, "ZmMissingEntitiesT6");

        REQUIRE_FALSE(linkerResult);
    }

    TEST_CASE("T6 zombies custom map loads zbarrier dependencies from entities", "[t6][system][zm][map]")
    {
        const auto testDir = oat::paths::GetSystemTestsDirectory() / "Game/T6/ZmCustomMapRequirements";
        const auto outputPath = oat::paths::GetTempDirectory("ZmCustomMapMissingZBarrierT6");

        const auto linkerResult = RunLinkerForZone(
            testDir, {testDir / "ZmMissingZBarrierT6", testDir / "CommonAssets"}, outputPath, "ZmMissingZBarrierT6");

        REQUIRE_FALSE(linkerResult);
    }
} // namespace
