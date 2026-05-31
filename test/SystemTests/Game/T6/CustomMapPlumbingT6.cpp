#include "Linker.h"
#include "OatTestPaths.h"
#include "SystemTestsPaths.h"
#include "Utils/Logging/Log.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace
{
    bool RunLinkerForZone(const fs::path& testDir, const fs::path& outputPath, const char* zoneName)
    {
        const auto testDirString = testDir.string();
        const auto outputPathString = outputPath.string();

        const char* argStrings[]{
            "SystemTests",
            "--verbose",
            "--asset-search-path",
            testDirString.c_str(),
            "--source-search-path",
            testDirString.c_str(),
            "--output-folder",
            outputPathString.c_str(),
            zoneName,
        };

        LinkerArgs args;

        bool shouldContinue = true;
        const auto couldParseArgs = args.ParseArgs(std::extent_v<decltype(argStrings)>, argStrings, shouldContinue);

        REQUIRE(couldParseArgs);
        REQUIRE(shouldContinue);

        con::reset_counts();

        const auto linker = Linker::Create(std::move(args));
        const auto result = linker->Start();

        con::reset_counts();

        return result;
    }

    TEST_CASE("T6 custom map target enters map backend without explicit assets", "[t6][system][map]")
    {
        const auto testDir = oat::paths::GetSystemTestsDirectory() / "Game/T6/CustomMapPlumbing/ValidSourceMarkers";
        const auto outputPath = oat::paths::GetTempDirectory("T6CustomMapBackendNotImplemented");

        const auto linkerResult = RunLinkerForZone(testDir, outputPath, "zm_custom_map_shell");

        REQUIRE_FALSE(linkerResult);
        REQUIRE_FALSE(fs::exists(outputPath / "zm_custom_map_shell.ff"));
    }

    TEST_CASE("T6 custom map target reports missing required source markers", "[t6][system][map]")
    {
        const auto testDir = oat::paths::GetSystemTestsDirectory() / "Game/T6/CustomMapPlumbing/MissingSourceMarkers";
        const auto outputPath = oat::paths::GetTempDirectory("T6CustomMapMissingSourceMarkers");

        const auto linkerResult = RunLinkerForZone(testDir, outputPath, "zm_missing_map_source");

        REQUIRE_FALSE(linkerResult);
        REQUIRE_FALSE(fs::exists(outputPath / "zm_missing_map_source.ff"));
    }
} // namespace
