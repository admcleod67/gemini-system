#include <doctest/doctest.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>

#include "FileSystem.h"
#include "VocResolver.h"

static std::filesystem::path uniqueVocTempDir() {
    auto base = std::filesystem::temp_directory_path();
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return base / ("pick-voc-test-" + std::to_string(tick));
}

TEST_CASE("VocResolver parses F/Q/V records from VOC and resolves program file with fallback") {
    const auto root = uniqueVocTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("BP", "001 F\n002 BP\n003 /gemini/fs/DM/BP\n"));
    fs.write("VOC", PickFS::Record("DEFAULT", "Q\nBP\n"));
    fs.write("VOC", PickFS::Record("BASIC", "V\nBASIC\n"));

    PickVoc::VocResolver resolver(fs);
    const auto fromQ = resolver.resolveProgramLocation("DEFAULT");
    CHECK(fromQ.fileName == "BP");
    CHECK(fromQ.recordKey == "DEFAULT");

    const auto fallback = resolver.resolveProgramLocation("HELLO");
    CHECK(fallback.fileName == "BP");
    CHECK(fallback.recordKey == "HELLO");
}

TEST_CASE("VocResolver lookup is case-insensitive and protects against Q-cycles") {
    const auto root = uniqueVocTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("bp", "F\nBP\n"));
    fs.write("VOC", PickFS::Record("A", "Q\nB\n"));
    fs.write("VOC", PickFS::Record("B", "Q\nA\n"));

    PickVoc::VocResolver resolver(fs);
    const auto caseInsensitive = resolver.resolveProgramLocation("Bp");
    CHECK(caseInsensitive.fileName == "BP");

    const auto cycleFallback = resolver.resolveProgramLocation("A");
    CHECK(cycleFallback.fileName == "BP");
    CHECK(cycleFallback.recordKey == "A");
}

TEST_CASE("VocResolver resolves PROC scripts with PROC fallback") {
    const auto root = uniqueVocTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("SCRIPT", "F\nSCRIPTS\n"));

    PickVoc::VocResolver resolver(fs);
    const auto mapped = resolver.resolveProcScriptLocation("SCRIPT");
    CHECK(mapped.fileName == "SCRIPTS");
    CHECK(mapped.recordKey == "SCRIPT");

    const auto fallback = resolver.resolveProcScriptLocation("MISSING");
    CHECK(fallback.fileName == "PROC");
    CHECK(fallback.recordKey == "MISSING");
}

TEST_CASE("VocResolver resolves V-type verb aliases for PROC TCL bridge") {
    const auto root = uniqueVocTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("SAYWHO", "V\nWHO\n"));

    PickVoc::VocResolver resolver(fs);
    CHECK(resolver.resolveVerbName("SAYWHO") == "WHO");
    CHECK(resolver.resolveVerbName("saywho") == "WHO");
    CHECK(resolver.resolveVerbName("UNKNOWN") == "UNKNOWN");
}

TEST_CASE("VocResolver D and A file pointers resolve like F for programs") {
    const auto root = uniqueVocTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("FROM_D", "D\nCUSTOM\n"));
    fs.write("VOC", PickFS::Record("FROM_A", "A\nOTHER\n"));

    PickVoc::VocResolver resolver(fs);
    const auto locD = resolver.resolveProgramLocation("FROM_D");
    CHECK(locD.fileName == "CUSTOM");
    CHECK(locD.recordKey == "FROM_D");
    const auto locA = resolver.resolveProgramLocation("FROM_A");
    CHECK(locA.fileName == "OTHER");
    CHECK(locA.recordKey == "FROM_A");
}

TEST_CASE("VocResolver X-type verb alias matches V behaviour") {
    const auto root = uniqueVocTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("SHOUT", "X\nECHO\n"));

    PickVoc::VocResolver resolver(fs);
    CHECK(resolver.resolveVerbName("SHOUT") == "ECHO");
}

TEST_CASE("VocResolver resolveVerbName follows Q then V and V chains") {
    const auto root = uniqueVocTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("USEWHO", "Q\nCHAIN\n"));
    fs.write("VOC", PickFS::Record("CHAIN", "V\nWHO\n"));
    fs.write("VOC", PickFS::Record("ALIAS", "V\nMID\n"));
    fs.write("VOC", PickFS::Record("MID", "V\nWHO\n"));

    PickVoc::VocResolver resolver(fs);
    CHECK(resolver.resolveVerbName("USEWHO") == "WHO");
    CHECK(resolver.resolveVerbName("ALIAS") == "WHO");
}

TEST_CASE("VocResolver resolveVerbName Q cycle returns original token") {
    const auto root = uniqueVocTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("QA", "Q\nQB\n"));
    fs.write("VOC", PickFS::Record("QB", "Q\nQA\n"));

    PickVoc::VocResolver resolver(fs);
    CHECK(resolver.resolveVerbName("QA") == "QA");
}

TEST_CASE("VocResolver resolveVerbName stops after max hops on long Q chain") {
    const auto root = uniqueVocTempDir();
    PickFS::FileSystem fs(root);
    fs.createFile("VOC");
    for (int i = 0; i < 70; ++i) {
        const std::string name = "L" + std::to_string(i);
        const std::string next = "L" + std::to_string(i + 1);
        fs.write("VOC", PickFS::Record(name, "Q\n" + next + "\n"));
    }

    PickVoc::VocResolver resolver(fs);
    CHECK(resolver.resolveVerbName("L0") == "L0");
}

#ifndef PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE
#define PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE ""
#endif

TEST_CASE("committed bootstrap SYSPROG fixture VOC resolves ED to EDIT and BP program fallback") {
    if (std::strlen(PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE) == 0) {
        return;
    }
    const std::filesystem::path root(PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE);
    if (!std::filesystem::exists(root / "VOC" / "ED.item")) {
        return;
    }

    PickFS::FileSystem fs(root);
    PickVoc::VocResolver resolver(fs);
    CHECK(resolver.resolveVerbName("ED") == "EDIT");
    CHECK(resolver.resolveVerbName("ed") == "EDIT");

    const auto loc = resolver.resolveProgramLocation("ANYPRG");
    CHECK(loc.fileName == "BP");
    CHECK(loc.recordKey == "ANYPRG");
}
