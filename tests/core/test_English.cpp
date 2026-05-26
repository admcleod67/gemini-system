#include <doctest/doctest.h>

#include <chrono>
#include <cstdio>
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

TEST_CASE("english file-scoped DICT-DATA overrides global DICT") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.createFile("DICT-DATA");
    fs.createFile("DICT");
    fs.write("DICT-DATA", PickFS::Record("NM", "A\n1\n"));
    fs.write("DICT", PickFS::Record("NM", "A\n2\n"));
    fs.write("DATA", PickFS::Record("R1", "ATTR1_VAL\nATTR2_VAL\n"));

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    std::string error;
    const auto res = svc.run(fs, {"LIST", "DATA", "NM"}, pc, eo, error);
    REQUIRE(res.has_value());
    CHECK(error.empty());
    REQUIRE(!res->lines.empty());
    CHECK(res->lines[0].find("ATTR1_VAL") != std::string::npos);
    CHECK(res->lines[0].find("ATTR2_VAL") == std::string::npos);
}

TEST_CASE("english LIST unknown field yields error") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DATA", PickFS::Record("R1", "A\n"));

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    std::string error;
    CHECK_FALSE(svc.run(fs, {"LIST", "DATA", "UNKNOWNFIELD"}, pc, eo, error).has_value());
    CHECK(error.find("Unknown ENGLISH field") != std::string::npos);
    CHECK(error.find("UNKNOWNFIELD") != std::string::npos);
}

TEST_CASE("english parser accepts HEADING clause") {
    PickCore::English::EnglishParser parser;
    std::string error;
    const PickCore::English::ParseContext pc;

    const auto withHeading = parser.parse({"LIST", "CUSTOMERS", "HEADING", "Hello"}, pc, error);
    REQUIRE(withHeading.has_value());
    CHECK(error.empty());
    REQUIRE(withHeading->heading.has_value());
    CHECK(*withHeading->heading == "Hello");

    const auto noHeading = parser.parse({"LIST", "CUSTOMERS"}, pc, error);
    REQUIRE(noHeading.has_value());
    CHECK_FALSE(noHeading->heading.has_value());
}

TEST_CASE("english parser HEADING rejects missing literal") {
    PickCore::English::EnglishParser parser;
    std::string error;
    const PickCore::English::ParseContext pc;
    const auto q = parser.parse({"LIST", "CUSTOMERS", "HEADING"}, pc, error);
    CHECK_FALSE(q.has_value());
    CHECK(error == "HEADING requires a quoted string");
}

TEST_CASE("english parser HEADING rejects structural keyword as argument") {
    PickCore::English::EnglishParser parser;
    std::string error;
    const PickCore::English::ParseContext pc;
    const auto q = parser.parse({"LIST", "CUSTOMERS", "HEADING", "BY", "NAME"}, pc, error);
    CHECK_FALSE(q.has_value());
    CHECK(error == "HEADING requires a quoted string");
}

TEST_CASE("english parser HEADING rejects duplicates") {
    PickCore::English::EnglishParser parser;
    std::string error;
    const PickCore::English::ParseContext pc;
    const auto q = parser.parse({"LIST", "CUSTOMERS", "HEADING", "A", "HEADING", "B"}, pc, error);
    CHECK_FALSE(q.has_value());
    CHECK(error == "HEADING can only appear once");
}

TEST_CASE("english parser HEADING before BY clause works on SORT") {
    PickCore::English::EnglishParser parser;
    std::string error;
    const PickCore::English::ParseContext pc;
    const auto q = parser.parse({"SORT", "CUSTOMERS", "HEADING", "Report", "BY", "NAME"}, pc, error);
    REQUIRE(q.has_value());
    CHECK(error.empty());
    REQUIRE(q->heading.has_value());
    CHECK(*q->heading == "Report");
    REQUIRE(q->sortKeys.size() == 1);
    CHECK(q->sortKeys[0].fieldToken == "NAME");
}

TEST_CASE("english parser HEADING after BY clause also works (positional flexibility)") {
    PickCore::English::EnglishParser parser;
    std::string error;
    const PickCore::English::ParseContext pc;
    const auto q = parser.parse({"SORT", "CUSTOMERS", "BY", "NAME", "HEADING", "After"}, pc, error);
    REQUIRE(q.has_value());
    REQUIRE(q->heading.has_value());
    CHECK(*q->heading == "After");
    REQUIRE(q->sortKeys.size() == 1);
    CHECK(q->sortKeys[0].fieldToken == "NAME");
}

TEST_CASE("english parser HEADING on bare COUNT preserves heading") {
    PickCore::English::EnglishParser parser;
    std::string error;
    PickCore::English::ParseContext pc;
    pc.implicitFile = true;
    pc.imposedFileName = "DATA";
    const auto q = parser.parse({"COUNT", "HEADING", "Summary"}, pc, error);
    REQUIRE(q.has_value());
    CHECK(q->verb == PickCore::English::Verb::COUNT);
    CHECK(q->fileName == "DATA");
    REQUIRE(q->heading.has_value());
    CHECK(*q->heading == "Summary");
}

TEST_CASE("english service LIST with HEADING emits heading as first line") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    fs.write("DATA", PickFS::Record("R1", "ALICE\n"));
    fs.write("DATA", PickFS::Record("R2", "BOB\n"));

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    std::string error;
    const auto res = svc.run(fs, {"LIST", "DATA", "NAME", "HEADING", "Customers"}, pc, eo, error);
    REQUIRE(res.has_value());
    CHECK(error.empty());
    REQUIRE(res->lines.size() == 3);
    CHECK(res->lines[0] == "Customers");
    CHECK(res->lines[1].find("ALICE") != std::string::npos);
    CHECK(res->lines[2].find("BOB") != std::string::npos);
}

TEST_CASE("english service COUNT with HEADING renders heading then count") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record("R1", "A\n"));
    fs.write("DATA", PickFS::Record("R2", "B\n"));

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    std::string error;
    const auto res = svc.run(fs, {"COUNT", "DATA", "HEADING", "Summary"}, pc, eo, error);
    REQUIRE(res.has_value());
    REQUIRE(res->lines.size() == 2);
    CHECK(res->lines[0] == "Summary");
    CHECK(res->lines[1] == "2");
}

TEST_CASE("english service LIST with HEADING paginates via EnglishRunOptions::pageLength") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    // Seed 15 records (ids R01..R15 for stable lexical order).
    for (int i = 1; i <= 15; ++i) {
        char id[8]{};
        std::snprintf(id, sizeof id, "R%02d", i);
        const std::string val = "N" + std::to_string(i) + "\n";
        fs.write("DATA", PickFS::Record(id, val));
    }

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    eo.pageLength = 5;
    std::string error;
    const auto res = svc.run(fs, {"LIST", "DATA", "NAME", "HEADING", "Customers"}, pc, eo, error);
    REQUIRE(res.has_value());
    CHECK(error.empty());

    // Layout: page 1 = heading + 4 rows (5 lines); each subsequent page = blank + heading + 4 rows (6 lines).
    // 15 rows -> 4 pages (1, 5, 9, 13 starting rows). Last page has 3 rows.
    // 5 + 6 + 6 + 5 = 22 lines.
    REQUIRE(res->lines.size() == 22);
    CHECK(res->lines[0] == "Customers");      // page 1 heading
    CHECK(res->lines[5] == "");                // separator
    CHECK(res->lines[6] == "Customers");      // page 2 heading
    CHECK(res->lines[11] == "");
    CHECK(res->lines[12] == "Customers");
    CHECK(res->lines[17] == "");
    CHECK(res->lines[18] == "Customers");
    // Last row of last page is row 15 -> ends with N15.
    CHECK(res->lines.back().find("N15") != std::string::npos);
}

TEST_CASE("english service LIST without HEADING ignores EnglishRunOptions::pageLength") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    for (int i = 1; i <= 12; ++i) {
        char id[8]{};
        std::snprintf(id, sizeof id, "R%02d", i);
        fs.write("DATA", PickFS::Record(id, "N\n"));
    }

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    eo.pageLength = 3;
    std::string error;
    const auto res = svc.run(fs, {"LIST", "DATA", "NAME"}, pc, eo, error);
    REQUIRE(res.has_value());
    // 12 rows; pagination disabled because there is no HEADING -> 12 lines, no blanks.
    REQUIRE(res->lines.size() == 12);
    for (const std::string &line: res->lines) {
        CHECK_FALSE(line.empty());
    }
}

TEST_CASE("english service SELECT with HEADING renders heading then summary") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record("R1", "A\n"));

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    std::string error;
    const auto res = svc.run(fs, {"SELECT", "DATA", "HEADING", "Pick"}, pc, eo, error);
    REQUIRE(res.has_value());
    REQUIRE(res->lines.size() == 2);
    CHECK(res->lines[0] == "Pick");
    CHECK(res->lines[1] == "1 records selected");
    CHECK(res->selectedIds.size() == 1);
}

TEST_CASE("english SORT unknown BY field yields error") {
    const auto root = uniqueEnglishTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DATA", PickFS::Record("R1", "X\n"));

    PickCore::English::EnglishService svc;
    PickCore::English::ParseContext pc;
    PickCore::English::EnglishRunOptions eo;
    std::string error;
    CHECK_FALSE(svc.run(fs, {"SORT", "DATA", "BY", "NOSUCHKEY"}, pc, eo, error).has_value());
    CHECK(error.find("Unknown ENGLISH field") != std::string::npos);
}
