#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <string>

#include "FileSystem.h"
#include "LockTable.h"

using PickCore::Locking::LockTable;
using PickCore::Locking::LockType;

static std::filesystem::path uniqueFsTempDir() {
    auto base = std::filesystem::temp_directory_path();
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return base / ("pick-fs-lock-test-" + std::to_string(tick));
}

static void seedRecord(PickFS::FileSystem &fs, const std::string &recordName, const std::string &value = "DATA") {
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record(recordName, value));
}

TEST_CASE("FileSystem S1 readU blocks S2 read on same record") {
    const auto root = uniqueFsTempDir();
    const auto locks = std::make_shared<LockTable>();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    fsA.setLockContext(locks, "S1");
    fsB.setLockContext(locks, "S2");
    seedRecord(fsA, "R1");

    REQUIRE(fsA.readU("DATA", "R1").has_value());

    CHECK_THROWS_AS(fsB.read("DATA", "R1"), PickFS::FileSystemError);
    try {
        (void) fsB.read("DATA", "R1");
    } catch (const PickFS::FileSystemError &ex) {
        CHECK(std::string(ex.what()).find("RECORD LOCKED") != std::string::npos);
    }
}

TEST_CASE("FileSystem same session readU then read succeeds") {
    const auto root = uniqueFsTempDir();
    const auto locks = std::make_shared<LockTable>();
    PickFS::FileSystem fs(root);
    fs.setLockContext(locks, "S1");
    seedRecord(fs, "R1");

    REQUIRE(fs.readU("DATA", "R1").has_value());
    const auto readBack = fs.read("DATA", "R1");
    REQUIRE(readBack.has_value());
    CHECK(readBack->value() == "DATA");
}

TEST_CASE("FileSystem S1 writeU blocks S2 write on same record") {
    const auto root = uniqueFsTempDir();
    const auto locks = std::make_shared<LockTable>();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    fsA.setLockContext(locks, "S1");
    fsB.setLockContext(locks, "S2");
    seedRecord(fsA, "R1");

    fsA.writeU("DATA", PickFS::Record("R1", "LOCKED"));

    CHECK_THROWS_AS(fsB.write("DATA", PickFS::Record("R1", "OTHER")), PickFS::FileSystemError);
}

TEST_CASE("FileSystem releaseRecord allows other session to readU") {
    const auto root = uniqueFsTempDir();
    const auto locks = std::make_shared<LockTable>();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    fsA.setLockContext(locks, "S1");
    fsB.setLockContext(locks, "S2");
    seedRecord(fsA, "R1");

    REQUIRE(fsA.readU("DATA", "R1").has_value());
    CHECK(fsA.releaseRecord("DATA", "R1"));
    CHECK(fsB.readU("DATA", "R1").has_value());
}

TEST_CASE("FileSystem S2 readU conflicts when S1 holds lock") {
    const auto root = uniqueFsTempDir();
    const auto locks = std::make_shared<LockTable>();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    fsA.setLockContext(locks, "S1");
    fsB.setLockContext(locks, "S2");
    seedRecord(fsA, "R1");

    REQUIRE(fsA.readU("DATA", "R1").has_value());
    CHECK_THROWS_AS(fsB.readU("DATA", "R1"), PickFS::FileSystemError);
}

TEST_CASE("FileSystem releaseAllForSession allows other session to acquire") {
    const auto root = uniqueFsTempDir();
    const auto locks = std::make_shared<LockTable>();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    fsA.setLockContext(locks, "S1");
    fsB.setLockContext(locks, "S2");
    seedRecord(fsA, "R1");

    REQUIRE(fsA.readU("DATA", "R1").has_value());
    CHECK(locks->releaseAllForSession("S1") == 1);
    CHECK(fsB.readU("DATA", "R1").has_value());
}

TEST_CASE("FileSystem deleteFile clears locks on that file only") {
    const auto root = uniqueFsTempDir();
    const auto locks = std::make_shared<LockTable>();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    fsA.setLockContext(locks, "S1");
    fsB.setLockContext(locks, "S2");
    seedRecord(fsA, "R1");
    fsA.createFile("OTHER");
    fsA.write("OTHER", PickFS::Record("X", "Y"));
    REQUIRE(fsA.readU("DATA", "R1").has_value());
    REQUIRE(fsA.readU("OTHER", "X").has_value());

    fsA.deleteFile("DATA");

    CHECK_FALSE(locks->lookup("DATA", "R1").has_value());
    REQUIRE(locks->lookup("OTHER", "X").has_value());
    CHECK(locks->lookup("OTHER", "X")->ownerSessionId == "S1");

    CHECK(locks->tryAcquire("S2", "DATA", "R1", LockType::ReadU));
    CHECK_FALSE(locks->tryAcquire("S2", "OTHER", "X", LockType::ReadU));
}

TEST_CASE("FileSystem without lock context behaves unchanged") {
    const auto root = uniqueFsTempDir();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    seedRecord(fsA, "R1");

    const auto a = fsA.read("DATA", "R1");
    const auto b = fsB.read("DATA", "R1");
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(a->value() == b->value());

    fsA.write("DATA", PickFS::Record("R1", "UPDATED"));
    const auto after = fsB.read("DATA", "R1");
    REQUIRE(after.has_value());
    CHECK(after->value() == "UPDATED");
}

TEST_CASE("FileSystem writeU idempotent same session upgrades lock type") {
    const auto root = uniqueFsTempDir();
    const auto locks = std::make_shared<LockTable>();
    PickFS::FileSystem fs(root);
    fs.setLockContext(locks, "S1");
    seedRecord(fs, "R1");

    REQUIRE(fs.readU("DATA", "R1").has_value());
    fs.writeU("DATA", PickFS::Record("R1", "WRITEU"));

    const auto entry = locks->lookup("DATA", "R1");
    REQUIRE(entry.has_value());
    CHECK(entry->lockType == LockType::WriteU);
    CHECK(entry->ownerSessionId == "S1");
}

TEST_CASE("FileSystem deleteRecord releases session lock") {
    const auto root = uniqueFsTempDir();
    const auto locks = std::make_shared<LockTable>();
    PickFS::FileSystem fs(root);
    fs.setLockContext(locks, "S1");
    seedRecord(fs, "R1");

    REQUIRE(fs.readU("DATA", "R1").has_value());
    const PickFS::FileSystem::FileHandle handle = fs.openFile("DATA");
    fs.deleteRecord(handle, "R1");
    CHECK_FALSE(locks->lookup("DATA", "R1").has_value());
}
