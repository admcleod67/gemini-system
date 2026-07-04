#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <memory>

#include "FileSystem.h"
#include "GeminiSession.h"
#include "LockTable.h"

using PickCore::Locking::LockTable;

static std::filesystem::path uniqueFsTempDir() {
    auto base = std::filesystem::temp_directory_path();
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return base / ("pick-shell-lock-test-" + std::to_string(tick));
}

TEST_CASE("GeminiSession clearLoginSession releases record locks") {
    PickShell::GeminiSession session;
    const auto locks = std::make_shared<LockTable>();
    session.setSharedLockTable(locks);

    const auto root = uniqueFsTempDir();
    session.setFileSystemRoot(root);
    session.bindLockSession("S1");

    session.fileSystem().createFile("DATA");
    session.fileSystem().write("DATA", PickFS::Record("R1", "X"));
    REQUIRE(session.fileSystem().readU("DATA", "R1").has_value());
    REQUIRE(locks->lookup("DATA", "R1").has_value());

    session.clearLoginSession();
    CHECK_FALSE(locks->lookup("DATA", "R1").has_value());
    CHECK(session.sessionLockId().empty());

    PickFS::FileSystem fsOther(root);
    fsOther.setLockContext(locks, "S2");
    CHECK(fsOther.readU("DATA", "R1").has_value());
}

TEST_CASE("GeminiSession resetForQuit releases record locks") {
    PickShell::GeminiSession session;
    const auto locks = std::make_shared<LockTable>();
    session.setSharedLockTable(locks);

    const auto root = uniqueFsTempDir();
    session.setFileSystemRoot(root);
    session.bindLockSession("S1");

    session.fileSystem().createFile("DATA");
    session.fileSystem().write("DATA", PickFS::Record("R1", "X"));
    REQUIRE(session.fileSystem().readU("DATA", "R1").has_value());

    session.resetForQuit();
    CHECK_FALSE(locks->lookup("DATA", "R1").has_value());
}

TEST_CASE("GeminiSession makeSessionLockId format") {
    CHECK(PickShell::GeminiSession::makeSessionLockId(23, "ACCT", "USER") == "S:23:ACCT:USER");
}
