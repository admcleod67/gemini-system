#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>

#include "EnglishParser.h"
#include "EnglishService.h"
#include "FileSystem.h"

static std::filesystem::path uniqueEnglishTempDir() {
    const auto base = std::filesystem::temp_directory_path();
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return base / ("pick-english-test-" + std::to_string(tick));
}

TEST_CASE("english parser accepts LIST COUNT SELECT forms") {
    PickCore::English::EnglishParser parser;
    std::string error;
    const auto listQ = parser.parse({"LIST", "DATA", "NAME"}, error);
    REQUIRE(listQ.has_value());
    CHECK(listQ->fileName == "DATA");
    CHECK(listQ->fields.size() == 1);

    const auto countQ = parser.parse({"COUNT", "DATA"}, error);
    REQUIRE(countQ.has_value());
    CHECK(countQ->fields.empty());

    const auto selQ = parser.parse({"SELECT", "DATA", "1"}, error);
    REQUIRE(selQ.has_value());
    CHECK(selQ->fields.size() == 1);
}

TEST_CASE("english service lists and counts from structured records") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    fs.write("DATA", PickFS::Record("R1", "ALICE\nLONDON\n"));
    fs.write("DATA", PickFS::Record("R2", "BOB\nPARIS\n"));

    PickCore::English::EnglishService svc;
    std::string error;
    const auto listRes = svc.run(fs, {"LIST", "DATA", "NAME"}, error);
    REQUIRE(listRes.has_value());
    CHECK(error.empty());
    REQUIRE(listRes->lines.size() == 2);
    CHECK(listRes->lines[0].find("ALICE") != std::string::npos);
    CHECK(listRes->lines[1].find("BOB") != std::string::npos);

    const auto countRes = svc.run(fs, {"COUNT", "DATA"}, error);
    REQUIRE(countRes.has_value());
    CHECK(countRes->lines.size() == 1);
    CHECK(countRes->lines[0] == "2");
}

TEST_CASE("english service SELECT returns selected IDs") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record("R1", "A\n"));
    fs.write("DATA", PickFS::Record("R2", "B\n"));

    PickCore::English::EnglishService svc;
    std::string error;
    const auto res = svc.run(fs, {"SELECT", "DATA"}, error);
    REQUIRE(res.has_value());
    CHECK(error.empty());
    CHECK(res->selectedIds.size() == 2);
    CHECK(res->lines.size() == 1);
    CHECK(res->lines[0] == "2 records selected");
}
