#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <memory>

#include "LockTable.h"
#include "ShellSession.h"

using PickCore::Locking::LockTable;

static std::filesystem::path uniqueFsTempDir() {
    auto base = std::filesystem::temp_directory_path();
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return base / ("pick-shell-lock-test-" + std::to_string(tick));
}

TEST_CASE("ShellSession clearLoginSession releases record locks") {
    PickVM::Runtime rt;
    PickShell::ShellSession session(rt);
    const auto locks = std::make_shared<LockTable>();
    session.setSharedLockTable(locks);

    const auto root = uniqueFsTempDir();
    session.setFileSystemRoot(root);
    session.bindLockSession("S1");

    session.fileSystem_.createFile("DATA");
    session.fileSystem_.write("DATA", PickFS::Record("R1", "X"));
    REQUIRE(session.fileSystem_.readU("DATA", "R1").has_value());
    REQUIRE(locks->lookup("DATA", "R1").has_value());

    session.clearLoginSession();
    CHECK_FALSE(locks->lookup("DATA", "R1").has_value());
    CHECK(session.sessionLockId().empty());

    PickFS::FileSystem fsOther(root);
    fsOther.setLockContext(locks, "S2");
    CHECK(fsOther.readU("DATA", "R1").has_value());
}

TEST_CASE("ShellSession resetForQuit releases record locks") {
    PickVM::Runtime rt;
    PickShell::ShellSession session(rt);
    const auto locks = std::make_shared<LockTable>();
    session.setSharedLockTable(locks);

    const auto root = uniqueFsTempDir();
    session.setFileSystemRoot(root);
    session.bindLockSession("S1");

    session.fileSystem_.createFile("DATA");
    session.fileSystem_.write("DATA", PickFS::Record("R1", "X"));
    REQUIRE(session.fileSystem_.readU("DATA", "R1").has_value());

    session.resetForQuit();
    CHECK_FALSE(locks->lookup("DATA", "R1").has_value());
}

TEST_CASE("ShellSession makeSessionLockId format") {
    CHECK(PickShell::ShellSession::makeSessionLockId(23, "ACCT", "USER") == "S:23:ACCT:USER");
}
