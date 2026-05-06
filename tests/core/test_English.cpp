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
    const PickCore::English::ParseContext pc;
    const auto listQ = parser.parse({"LIST", "DATA", "NAME"}, pc, error);
    REQUIRE(listQ.has_value());
    CHECK(listQ->fileName == "DATA");
    CHECK(listQ->fields.size() == 1);

    const auto countQ = parser.parse({"COUNT", "DATA"}, pc, error);
    REQUIRE(countQ.has_value());
    CHECK(countQ->fields.empty());

    const auto selQ = parser.parse({"SELECT", "DATA", "1"}, pc, error);
    REQUIRE(selQ.has_value());
    CHECK(selQ->fields.size() == 1);

    CHECK_FALSE(parser.parse({"LIST", "DATA", "NAME", "BY", "X"}, pc, error).has_value());

    const auto sortQ = parser.parse({"SORT", "DATA", "NAME", "BY", "NAME"}, pc, error);
    REQUIRE(sortQ.has_value());
    REQUIRE(sortQ->sortKeys.size() == 1);
    CHECK(sortQ->sortKeys[0].fieldToken == "NAME");
    CHECK_FALSE(sortQ->sortKeys[0].descending);
}

TEST_CASE("english parser implicit file uses imposed filename") {
    PickCore::English::EnglishParser parser;
    std::string error;
    PickCore::English::ParseContext implicit;
    implicit.implicitFile = true;
    implicit.imposedFileName = "DATA";
    const auto q = parser.parse({"LIST", "CITY"}, implicit, error);
    REQUIRE(q.has_value());
    CHECK(q->fileName == "DATA");
    REQUIRE(q->fields.size() == 1);
    CHECK(q->fields[0] == "CITY");
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
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    std::string error;
    const auto listRes = svc.run(fs, {"LIST", "DATA", "NAME"}, pc, eo, error);
    REQUIRE(listRes.has_value());
    CHECK(error.empty());
    REQUIRE(listRes->lines.size() == 2);
    CHECK(listRes->lines[0].find("ALICE") != std::string::npos);
    CHECK(listRes->lines[1].find("BOB") != std::string::npos);

    const auto countRes = svc.run(fs, {"COUNT", "DATA"}, pc, eo, error);
    REQUIRE(countRes.has_value());
    CHECK(countRes->lines.size() == 1);
    CHECK(countRes->lines[0] == "2");
}

TEST_CASE("english service SORT BY field orders rows") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NM", "A\n1\n"));
    fs.write("DATA", PickFS::Record("R1", "ZEBRA\n"));
    fs.write("DATA", PickFS::Record("R2", "ADAM\n"));

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    std::string error;
    const auto res = svc.run(fs, {"SORT", "DATA", "NM", "BY", "NM"}, pc, eo, error);
    REQUIRE(res.has_value());
    CHECK(error.empty());
    REQUIRE(res->lines.size() == 2);
    CHECK(res->lines[0].find("ADAM") != std::string::npos);
    CHECK(res->lines[1].find("ZEBRA") != std::string::npos);
}

TEST_CASE("english service SORT without BY sorts by item id") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record("Z9", "A\n"));
    fs.write("DATA", PickFS::Record("Z10", "B\n"));

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    std::string error;
    const auto res = svc.run(fs, {"SORT", "DATA"}, pc, eo, error);
    REQUIRE(res.has_value());
    REQUIRE(res->lines.size() == 2);
    CHECK(res->lines[0].find("Z10") != std::string::npos); // lexical id order
}

TEST_CASE("english service constrained ids respect active-list order before sort") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record("A", "1\n"));
    fs.write("DATA", PickFS::Record("B", "2\n"));
    fs.write("DATA", PickFS::Record("C", "3\n"));

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    eo.constrainRecordIds = std::vector<std::string>{"C", "A"};
    std::string error;
    const auto unsorted = svc.run(fs, {"LIST", "DATA", "1"}, pc, eo, error);
    REQUIRE(unsorted.has_value());
    REQUIRE(unsorted->lines.size() == 2);
    CHECK(unsorted->lines[0].front() == 'C');

    eo.constrainRecordIds = std::vector<std::string>{"C", "A"};
    const auto res = svc.run(fs, {"SORT", "DATA", "1"}, pc, eo, error);
    REQUIRE(res.has_value());
    REQUIRE(res->lines.size() == 2);
    CHECK(res->lines[0].front() == 'A');
}

TEST_CASE("english service SELECT returns selected IDs") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record("R1", "A\n"));
    fs.write("DATA", PickFS::Record("R2", "B\n"));

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    std::string error;
    const auto res = svc.run(fs, {"SELECT", "DATA"}, pc, eo, error);
    REQUIRE(res.has_value());
    CHECK(error.empty());
    CHECK(res->selectedIds.size() == 2);
    CHECK(res->lines.size() == 1);
    CHECK(res->lines[0] == "2 records selected");
}

TEST_CASE("english COUNT single token parses with implicit scope") {
    PickCore::English::EnglishParser parser;
    std::string error;
    PickCore::English::ParseContext pc;
    pc.implicitFile = true;
    pc.imposedFileName = "DATA";
    const auto q = parser.parse({"COUNT"}, pc, error);
    REQUIRE(q.has_value());
    CHECK(q->verb == PickCore::English::Verb::COUNT);
}
