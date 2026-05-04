#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "FileSystem.h"
#include "MdDefaultDataFile.h"

static std::filesystem::path uniqueMdTempDir() {
    const auto base = std::filesystem::temp_directory_path();
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return base / ("pick-md-def-" + std::to_string(tick));
}

TEST_CASE("loadDefaultDataFileFromMd returns first non-empty line when valid") {
    const auto root = uniqueMdTempDir();
    std::filesystem::create_directories(root / "MD");
    {
        std::ofstream f(root / "MD" / "DEFDATA.item");
        f << "\n  MYFILE  \nother\n";
    }
    PickFS::FileSystem fs(root);
    const auto v = PickCore::loadDefaultDataFileFromMd(fs);
    REQUIRE(v.has_value());
    CHECK(*v == "MYFILE");
}

TEST_CASE("loadDefaultDataFileFromMd rejects invalid file name") {
    const auto root = uniqueMdTempDir();
    std::filesystem::create_directories(root / "MD");
    {
        std::ofstream f(root / "MD" / "DEFDATA.item");
        f << "bad name\n";
    }
    PickFS::FileSystem fs(root);
    CHECK_FALSE(PickCore::loadDefaultDataFileFromMd(fs).has_value());
}

TEST_CASE("loadDefaultDataFileFromMd missing record returns nullopt") {
    const auto root = uniqueMdTempDir();
    std::filesystem::create_directories(root);
    PickFS::FileSystem fs(root);
    CHECK_FALSE(PickCore::loadDefaultDataFileFromMd(fs).has_value());
}
