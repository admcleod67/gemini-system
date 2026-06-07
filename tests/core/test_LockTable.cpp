#include <doctest/doctest.h>

#include "LockTable.h"

using PickCore::Locking::LockConflict;
using PickCore::Locking::LockTable;
using PickCore::Locking::LockType;
using PickCore::Locking::describeLockConflict;
using PickCore::Locking::lockTypeLabel;

TEST_CASE("LockTable acquire READU on empty record") {
    LockTable table;
    CHECK(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));

    const auto entry = table.lookup("DATA", "R1");
    REQUIRE(entry.has_value());
    CHECK(entry->ownerSessionId == "S1");
    CHECK(entry->lockType == LockType::ReadU);
}

TEST_CASE("LockTable acquire WRITEU on empty record") {
    LockTable table;
    CHECK(table.tryAcquire("S1", "DATA", "R1", LockType::WriteU));

    const auto entry = table.lookup("DATA", "R1");
    REQUIRE(entry.has_value());
    CHECK(entry->ownerSessionId == "S1");
    CHECK(entry->lockType == LockType::WriteU);
}

TEST_CASE("LockTable same session re-acquire is idempotent") {
    LockTable table;
    REQUIRE(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));
    CHECK(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));

    const auto entry = table.lookup("DATA", "R1");
    REQUIRE(entry.has_value());
    CHECK(entry->lockType == LockType::ReadU);
}

TEST_CASE("LockTable same session can change lock type READU to WRITEU") {
    LockTable table;
    REQUIRE(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));
    CHECK(table.tryAcquire("S1", "DATA", "R1", LockType::WriteU));

    const auto entry = table.lookup("DATA", "R1");
    REQUIRE(entry.has_value());
    CHECK(entry->lockType == LockType::WriteU);
}

TEST_CASE("LockTable same session can change lock type WRITEU to READU") {
    LockTable table;
    REQUIRE(table.tryAcquire("S1", "DATA", "R1", LockType::WriteU));
    CHECK(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));

    const auto entry = table.lookup("DATA", "R1");
    REQUIRE(entry.has_value());
    CHECK(entry->lockType == LockType::ReadU);
}

TEST_CASE("LockTable cross-session READU conflict") {
    LockTable table;
    REQUIRE(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));

    LockConflict conflict;
    CHECK_FALSE(table.tryAcquire("S2", "DATA", "R1", LockType::ReadU, &conflict));
    CHECK(conflict.fileName == "DATA");
    CHECK(conflict.recordId == "R1");
    CHECK(conflict.ownerSessionId == "S1");
    CHECK(conflict.lockType == LockType::ReadU);
}

TEST_CASE("LockTable cross-session READU vs WRITEU conflict") {
    LockTable table;
    REQUIRE(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));

    LockConflict conflict;
    CHECK_FALSE(table.tryAcquire("S2", "DATA", "R1", LockType::WriteU, &conflict));
    CHECK(conflict.ownerSessionId == "S1");
    CHECK(conflict.lockType == LockType::ReadU);
}

TEST_CASE("LockTable describeLockConflict contains RECORD LOCKED") {
    LockConflict conflict;
    conflict.fileName = "DATA";
    conflict.recordId = "R1";
    conflict.ownerSessionId = "S1";
    conflict.lockType = LockType::WriteU;

    const std::string message = describeLockConflict(conflict);
    CHECK(message.find("RECORD LOCKED") != std::string::npos);
    CHECK(message.find("S1") != std::string::npos);
    CHECK(message.find("WRITEU") != std::string::npos);
    CHECK(lockTypeLabel(LockType::ReadU) == "READU");
    CHECK(lockTypeLabel(LockType::WriteU) == "WRITEU");
}

TEST_CASE("LockTable release held lock") {
    LockTable table;
    REQUIRE(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));
    CHECK(table.release("S1", "DATA", "R1"));
    CHECK_FALSE(table.lookup("DATA", "R1").has_value());
}

TEST_CASE("LockTable release unheld or wrong session fails") {
    LockTable table;
    REQUIRE(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));
    CHECK_FALSE(table.release("S2", "DATA", "R1"));
    CHECK(table.lookup("DATA", "R1").has_value());

    CHECK_FALSE(table.release("S1", "DATA", "MISSING"));
    CHECK(table.lookup("DATA", "R1").has_value());
}

TEST_CASE("LockTable releaseAllForSession removes only that session") {
    LockTable table;
    REQUIRE(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));
    REQUIRE(table.tryAcquire("S1", "DATA", "R2", LockType::WriteU));
    REQUIRE(table.tryAcquire("S2", "DATA", "R3", LockType::ReadU));

    CHECK(table.releaseAllForSession("S1") == 2);
    CHECK_FALSE(table.lookup("DATA", "R1").has_value());
    CHECK_FALSE(table.lookup("DATA", "R2").has_value());
    CHECK(table.lookup("DATA", "R3").has_value());
}

TEST_CASE("LockTable releaseAllForFile removes all record locks on file") {
    LockTable table;
    REQUIRE(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));
    REQUIRE(table.tryAcquire("S2", "DATA", "R2", LockType::WriteU));
    REQUIRE(table.tryAcquire("S1", "OTHER", "R1", LockType::ReadU));

    CHECK(table.releaseAllForFile("DATA") == 2);
    CHECK_FALSE(table.lookup("DATA", "R1").has_value());
    CHECK_FALSE(table.lookup("DATA", "R2").has_value());
    CHECK(table.lookup("OTHER", "R1").has_value());
}

TEST_CASE("LockTable same record id in different files is independent") {
    LockTable table;
    CHECK(table.tryAcquire("S1", "DATA", "R1", LockType::ReadU));
    CHECK(table.tryAcquire("S2", "OTHER", "R1", LockType::WriteU));
    CHECK(table.lookup("DATA", "R1").has_value());
    CHECK(table.lookup("OTHER", "R1").has_value());
}

TEST_CASE("LockTable rejects empty session file or record id") {
    LockTable table;
    CHECK_FALSE(table.tryAcquire("", "DATA", "R1", LockType::ReadU));
    CHECK_FALSE(table.tryAcquire("S1", "", "R1", LockType::ReadU));
    CHECK_FALSE(table.tryAcquire("S1", "DATA", "", LockType::ReadU));
    CHECK_FALSE(table.lookup("DATA", "R1").has_value());
}
