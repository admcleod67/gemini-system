#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "FileSystem.h"

static std::filesystem::path uniqueFsTempDir() {
    auto base = std::filesystem::temp_directory_path();
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return base / ("pick-fs-test-" + std::to_string(tick));
}

TEST_CASE("filesystem stores default records as .item and preserves newline attributes") {
    const auto root = uniqueFsTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    const PickFS::FileSystem::FileHandle handle = fs.openFile("DATA");

    const std::string attributes = "NAME\n\nCITY\n";
    fs.writeRecord(handle, PickFS::Record("CUST1", attributes));

    CHECK(std::filesystem::exists(root / "DATA" / "CUST1.item"));
    const auto readBack = fs.readRecord(handle, "CUST1");
    REQUIRE(readBack.has_value());
    CHECK(readBack->value() == attributes);
}

TEST_CASE("filesystem uses typed extensions but lists logical item IDs only") {
    const auto root = uniqueFsTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("BP");
    const PickFS::FileSystem::FileHandle handle = fs.openFile("BP");

    fs.writeRecord(handle, PickFS::Record("HELLO", "10 PRINT \"HI\""));
    CHECK(std::filesystem::exists(root / "BP" / "HELLO.bas"));

    // Transitional backend may contain mixed extension artifacts; listing stays logical.
    std::ofstream(root / "BP" / "HELLO.item", std::ios::trunc) << "shadow";

    const std::vector<std::string> records = fs.listRecords(handle);
    REQUIRE(records.size() == 1);
    CHECK(records[0] == "HELLO");
}

TEST_CASE("filesystem deleteRecord removes stored record via file handle") {
    const auto root = uniqueFsTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    const PickFS::FileSystem::FileHandle handle = fs.openFile("DATA");

    fs.writeRecord(handle, PickFS::Record("ID1", "VALUE"));
    CHECK(std::filesystem::exists(root / "DATA" / "ID1.item"));

    fs.deleteRecord(handle, "ID1");
    CHECK_FALSE(std::filesystem::exists(root / "DATA" / "ID1.item"));

    bool missingThrown = false;
    try {
        fs.deleteRecord(handle, "ID1");
    } catch (const PickFS::FileSystemError &e) {
        missingThrown = std::string(e.what()).find("Record not found: ID1") != std::string::npos;
    }
    CHECK(missingThrown);
}
