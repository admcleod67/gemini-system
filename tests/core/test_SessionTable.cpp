#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "FileSystem.h"
#include "DaemonConfig.h"
#include "GeminiServiceDaemon.h"
#include "GeminiSessionHost.h"
#include "SerialSessionRunner.h"
#include "SessionTable.h"
#include "Shell.h"
#include "UserSession.h"

static std::filesystem::path uniqueTempDir() {
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("pick-session-table-test-" + std::to_string(tick));
}

namespace {
    [[nodiscard]] PickCore::UserSession makeUserSession(const std::filesystem::path &gem,
                                                          const std::filesystem::path &pickRoot,
                                                          const int port,
                                                          const std::string &username) {
        PickCore::UserSession session;
        session.catalogRoot = gem;
        session.pickRoot = pickRoot;
        session.accountName = "TST";
        session.username = username;
        session.whoPort = port;
        session.userNo = std::to_string(port);
        return session;
    }

    void seedDataRecord(const std::filesystem::path &pickRoot, const std::string &recordName, const std::string &value) {
        PickFS::FileSystem fs(pickRoot);
        fs.createFile("DATA");
        fs.write("DATA", PickFS::Record(recordName, value));
    }

    void handle(PickShell::Shell &shell, const std::string &line, std::ostringstream &out) {
        bool quit = false;
        shell.handleLine(line, out, quit);
    }

    [[nodiscard]] PickCore::GeminiServiceDaemon makeDaemon(const std::size_t maxSessions) {
        PickCore::DaemonConfig config{};
        config.maxSessions = maxSessions;
        return PickCore::GeminiServiceDaemon::create(config);
    }
} // namespace

TEST_CASE("SessionTable createSession wires shared lock table") {
    PickCore::GeminiServiceDaemon daemon = makeDaemon(2);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::SessionTable table(2);
    const PickShell::SessionHandle a = table.createSession(daemon);
    const PickShell::SessionHandle b = table.createSession(daemon);

    CHECK(a.session.hasSharedLockTable());
    CHECK(b.session.hasSharedLockTable());
    CHECK(a.id == 1);
    CHECK(b.id == 2);
    CHECK(a.session.whoPort() == 1);
    CHECK(b.session.whoPort() == 2);
}

TEST_CASE("SessionTable destroySession reuses freed port") {
    PickCore::GeminiServiceDaemon daemon = makeDaemon(2);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::SessionTable table(2);
    const PickShell::SessionHandle first = table.createSession(daemon);
    CHECK(first.id == 1);
    table.destroySession(first.id);

    const PickShell::SessionHandle second = table.createSession(daemon);
    CHECK(second.id == 1);
    CHECK(second.session.whoPort() == 1);
}

TEST_CASE("SessionTable attach uses daemon port for WHO identity") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pickRoot = gem / "accounts" / "TST";
    std::filesystem::create_directories(pickRoot / "MD");

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::createEmbedded();
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::SessionTable table(1);
    const PickShell::SessionHandle handle = table.createSession(daemon);
    PickCore::UserSession user;
    user.catalogRoot = gem;
    user.pickRoot = pickRoot;
    user.accountName = "TST";
    user.username = "USERA";
    user.whoPort = 99;
    handle.session.attach(user);

    CHECK(handle.session.whoPort() == 1);
    CHECK(handle.session.sessionLockId() == PickShell::GeminiSession::makeSessionLockId(1, "TST", "USERA"));

    std::ostringstream out;
    bool quit = false;
    handle.session.shell().handleLine("WHO", out, quit);
    CHECK(out.str() == "1 USERA TST\n");
}

TEST_CASE("SessionTable cross-session READU blocks peer READ") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pickRoot = gem / "accounts" / "TST";
    std::filesystem::create_directories(pickRoot / "MD");
    seedDataRecord(pickRoot, "R1", "seed");

    PickCore::GeminiServiceDaemon daemon = makeDaemon(2);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::SessionTable table(2);
    const PickShell::SessionHandle ha = table.createSession(daemon);
    const PickShell::SessionHandle hb = table.createSession(daemon);
    ha.session.shell().attachUserSession(makeUserSession(gem, pickRoot, 1, "USERA"));
    hb.session.shell().attachUserSession(makeUserSession(gem, pickRoot, 2, "USERB"));

    std::ostringstream outA;
    std::ostringstream outB;
    handle(ha.session.shell(), "READU DATA R1", outA);
    CHECK(outA.str() == "seed\n");
    handle(hb.session.shell(), "READ DATA R1", outB);
    CHECK(outB.str().find("RECORD LOCKED") != std::string::npos);
}

TEST_CASE("SessionTable destroySession releases locks") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pickRoot = gem / "accounts" / "TST";
    std::filesystem::create_directories(pickRoot / "MD");
    seedDataRecord(pickRoot, "R1", "seed");

    PickCore::GeminiServiceDaemon daemon = makeDaemon(2);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::SessionTable table(2);
    const PickShell::SessionHandle ha = table.createSession(daemon);
    const PickShell::SessionHandle hb = table.createSession(daemon);
    ha.session.shell().attachUserSession(makeUserSession(gem, pickRoot, 1, "USERA"));
    hb.session.shell().attachUserSession(makeUserSession(gem, pickRoot, 2, "USERB"));

    std::ostringstream outA;
    handle(ha.session.shell(), "READU DATA R1", outA);

    table.destroySession(ha.id);

    std::ostringstream outB;
    handle(hb.session.shell(), "READU DATA R1", outB);
    CHECK(outB.str() == "seed\n");
}

TEST_CASE("SerialSessionRunner excludes concurrent execution") {
    PickCore::SerialSessionRunner runner;
    std::atomic<bool> bStarted{false};
    std::atomic<bool> bBlockedDuringA{false};

    std::thread threadA([&] {
        runner.runExclusive(1, [&] {
            bStarted = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        });
    });

    std::thread threadB([&] {
        while (!bStarted.load()) {
            std::this_thread::yield();
        }
        std::atomic<bool> entered{false};
        std::thread waiter([&] {
            runner.runExclusive(2, [&] { entered = true; });
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        bBlockedDuringA = !entered.load();
        waiter.join();
    });

    threadA.join();
    threadB.join();
    CHECK(bBlockedDuringA.load());
}

TEST_CASE("SessionTable enforces maxSessions") {
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::createEmbedded();
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::SessionTable table(1);
    (void) table.createSession(daemon);
    CHECK_THROWS_AS((void) table.createSession(daemon), std::runtime_error);
}

TEST_CASE("GeminiSessionHost runExclusive serialises sessions") {
    PickCore::GeminiServiceDaemon daemon = makeDaemon(2);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::GeminiSessionHost host(daemon, 2);
    const PickShell::SessionHandle ha = host.createSession();
    const PickShell::SessionHandle hb = host.createSession();

    std::atomic<bool> bRan{false};
    std::thread blocker([&] {
        host.runExclusive(ha.id, [&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        });
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::thread waiter([&] {
        host.runExclusive(hb.id, [&] { bRan = true; });
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(!bRan.load());
    blocker.join();
    waiter.join();
    CHECK(bRan.load());
}
