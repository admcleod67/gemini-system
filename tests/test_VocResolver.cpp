#include <doctest/doctest.h>

#include <chrono>
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

    PickShell::VocResolver resolver(fs);
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

    PickShell::VocResolver resolver(fs);
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

    PickShell::VocResolver resolver(fs);
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

    PickShell::VocResolver resolver(fs);
    CHECK(resolver.resolveVerbName("SAYWHO") == "WHO");
    CHECK(resolver.resolveVerbName("saywho") == "WHO");
    CHECK(resolver.resolveVerbName("UNKNOWN") == "UNKNOWN");
}
