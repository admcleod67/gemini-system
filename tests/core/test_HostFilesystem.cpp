#include <doctest/doctest.h>

#include "PosixHostFilesystem.h"
#include "Runtime.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
    std::filesystem::path uniqueTempDir() {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto dir = std::filesystem::temp_directory_path() / ("pick-hostfs-" + std::to_string(tick));
        std::filesystem::create_directories(dir);
        return dir;
    }
} // namespace

TEST_CASE("PosixHostFilesystem write read close round-trip") {
    const auto dir = uniqueTempDir();
    const auto path = (dir / "note.txt").string();

    Gemini::PosixHostFilesystem fs;
    const int wh = fs.openWrite(path);
    CHECK(wh >= 1);
    CHECK(fs.write(wh, "hello") == 5);
    fs.close(wh);

    const int rh = fs.openRead(path);
    CHECK(fs.read(rh, 100) == "hello");
    CHECK(fs.read(rh, 100).empty()); // EOF
    fs.close(rh);
}

TEST_CASE("PosixHostFilesystem openRead missing file") {
    const auto dir = uniqueTempDir();
    const auto path = (dir / "missing.txt").string();
    Gemini::PosixHostFilesystem fs;
    CHECK_THROWS_WITH(fs.openRead(path), "HOSTFS: not found");
}

TEST_CASE("PosixHostFilesystem empty path") {
    Gemini::PosixHostFilesystem fs;
    CHECK_THROWS_WITH(fs.openRead(""), "HOSTFS: invalid path");
    CHECK_THROWS_WITH(fs.openWrite(""), "HOSTFS: invalid path");
    CHECK_THROWS_WITH(fs.openAppend(""), "HOSTFS: invalid path");
}

TEST_CASE("PosixHostFilesystem append and bad handle") {
    const auto dir = uniqueTempDir();
    const auto path = (dir / "log.txt").string();
    Gemini::PosixHostFilesystem fs;

    const int h1 = fs.openWrite(path);
    CHECK(fs.write(h1, "ab") == 2);
    fs.close(h1);

    const int h2 = fs.openAppend(path);
    CHECK(fs.write(h2, "cd") == 2);
    fs.close(h2);

    const int rh = fs.openRead(path);
    CHECK(fs.read(rh, 10) == "abcd");
    fs.close(rh);

    CHECK_THROWS_WITH(fs.close(rh), "HOSTFS: bad handle");
    CHECK_THROWS_WITH(fs.read(0, 1), "HOSTFS: bad handle");
}

TEST_CASE("Runtime hostFilesystem unbound by default") {
    PickVM::Runtime rt;
    CHECK(rt.hostFilesystem() == nullptr);
}
