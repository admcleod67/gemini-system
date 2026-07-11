#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <pick_system/version.hpp>

#include "DaemonConfig.h"
#include "FileSystem.h"
#include "GeminiServiceDaemon.h"
#include "GeminiSession.h"
#include "GeminiSessionHost.h"
#include "Shell.h"
#include "UserSession.h"

namespace {
    [[nodiscard]] std::filesystem::path uniqueTempDir() {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() / ("pick-admin-commands-" + std::to_string(tick));
    }

    [[nodiscard]] PickCore::GeminiServiceDaemon makeDaemon(const std::size_t maxSessions) {
        PickCore::DaemonConfig config{};
        config.maxSessions = maxSessions;
        return PickCore::GeminiServiceDaemon::create(config);
    }

    [[nodiscard]] PickCore::UserSession makeUser(const std::string &account,
                                                 const std::string &username,
                                                 const int port,
                                                 const std::filesystem::path &pickRoot = {}) {
        PickCore::UserSession user;
        user.accountName = account;
        user.username = username;
        user.whoPort = port;
        user.userNo = std::to_string(port);
        user.pickRoot = pickRoot.empty() ? std::filesystem::temp_directory_path() : pickRoot;
        return user;
    }

    void installHostQueries(PickShell::GeminiSessionHost &host, PickShell::GeminiSession &session) {
        session.shell().setAdminQueries(PickShell::ShellAdminQueries{
            .listSessions = [&host] { return host.listAdminSessions(); },
            .status = [&host] { return host.adminStatus(); },
            .killSession =
                [&host](const PickCore::SessionId port, std::string &error) {
                    if (host.sessions().find(port) == nullptr) {
                        error = "session not found";
                        return false;
                    }
                    host.retireSession(port);
                    host.destroySession(port);
                    return true;
                },
            .requestShutdown = [] {},
        });
    }

    void runLine(PickShell::Shell &shell, const std::string &line, std::ostringstream &out) {
        bool quit = false;
        shell.handleLine(line, out, quit);
    }

    [[nodiscard]] bool runLineQuit(PickShell::Shell &shell, const std::string &line, std::ostringstream &out) {
        bool quit = false;
        shell.handleLine(line, out, quit);
        return quit;
    }
} // namespace

TEST_CASE("LISTSESSIONS and STATUS list host sessions for SYSPROG") {
    PickCore::GeminiServiceDaemon daemon = makeDaemon(2);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::GeminiSessionHost host(daemon, 2);
    host.setAdminSocketPath("/tmp/gemini-admin-test.sock");
    host.setConsoleBoundQuery([](const PickCore::SessionId id) { return id == 1; });

    const PickShell::SessionHandle a = host.createSession();
    const PickShell::SessionHandle b = host.createSession();
    a.session.attach(makeUser("SYSPROG", "SYSPROG", 1));
    b.session.attach(makeUser("TST", "TST", 2));
    installHostQueries(host, a.session);
    installHostQueries(host, b.session);

    host.acquire(b.id);
    host.yieldWaitingForInput(b.id);
    host.acquire(a.id);

    std::ostringstream listOut;
    runLine(a.session.shell(), "LISTSESSIONS", listOut);
    const std::string list = listOut.str();
    CHECK(list.find("PORT BOUND USER ACCOUNT STATE\n") != std::string::npos);
    CHECK(list.find("1 yes SYSPROG SYSPROG Running\n") != std::string::npos);
    CHECK(list.find("2 no TST TST WaitingForInput\n") != std::string::npos);

    std::ostringstream statusOut;
    runLine(a.session.shell(), "STATUS", statusOut);
    const std::string status = statusOut.str();
    CHECK(status.find(std::string("Gemini System ") + pick_system::version_string + "\n") != std::string::npos);
    CHECK(status.find("sessions=2 maxSessions=2\n") != std::string::npos);
    CHECK(status.find("socket=/tmp/gemini-admin-test.sock\n") != std::string::npos);

    std::ostringstream systemStatusOut;
    runLine(a.session.shell(), "SYSTEM STATUS", systemStatusOut);
    CHECK(systemStatusOut.str() == status);

    host.release(a.id);
}

TEST_CASE("LISTSESSIONS and STATUS require SYSPROG account") {
    PickCore::GeminiServiceDaemon daemon = makeDaemon(1);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::GeminiSessionHost host(daemon, 1);
    const PickShell::SessionHandle session = host.createSession();
    installHostQueries(host, session.session);

    std::ostringstream loggedOut;
    runLine(session.session.shell(), "LISTSESSIONS", loggedOut);
    CHECK(loggedOut.str() == "LISTSESSIONS requires SYSPROG\n");

    loggedOut.str("");
    runLine(session.session.shell(), "STATUS", loggedOut);
    CHECK(loggedOut.str() == "STATUS requires SYSPROG\n");

    session.session.attach(makeUser("TST", "TST", 1));
    std::ostringstream denied;
    runLine(session.session.shell(), "LISTSESSIONS", denied);
    CHECK(denied.str() == "LISTSESSIONS requires SYSPROG\n");

    denied.str("");
    runLine(session.session.shell(), "STATUS", denied);
    CHECK(denied.str() == "STATUS requires SYSPROG\n");
}

TEST_CASE("LISTSESSIONS fallback works without admin queries for SYSPROG") {
    PickShell::GeminiSession gs;
    gs.attach(makeUser("SYSPROG", "allan", 0));

    std::ostringstream out;
    runLine(gs.shell(), "LISTSESSIONS", out);
    CHECK(out.str().find("PORT BOUND USER ACCOUNT STATE\n") != std::string::npos);
    CHECK(out.str().find("allan SYSPROG") != std::string::npos);

    out.str("");
    runLine(gs.shell(), "STATUS", out);
    CHECK(out.str().find("sessions=1 maxSessions=1\n") != std::string::npos);
    CHECK(out.str().find("socket=none\n") != std::string::npos);
}

TEST_CASE("KILLSESSION destroys peer session and releases locks for SYSPROG") {
    const auto root = uniqueTempDir();
    const auto pickRoot = root / "pick";
    std::filesystem::create_directories(pickRoot / "MD");
    {
        PickFS::FileSystem fs(pickRoot);
        fs.createFile("DATA");
        fs.write("DATA", PickFS::Record("R1", "seed"));
    }

    PickCore::GeminiServiceDaemon daemon = makeDaemon(2);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::GeminiSessionHost host(daemon, 2);
    const PickShell::SessionHandle a = host.createSession();
    const PickShell::SessionHandle b = host.createSession();
    const PickCore::SessionId portA = a.id;
    const PickCore::SessionId portB = b.id;
    a.session.attach(makeUser("SYSPROG", "SYSPROG", static_cast<int>(portA), pickRoot));
    b.session.attach(makeUser("TST", "TST", static_cast<int>(portB), pickRoot));
    installHostQueries(host, a.session);

    std::ostringstream lockOut;
    runLine(b.session.shell(), "READU DATA R1", lockOut);
    CHECK(lockOut.str() == "seed\n");

    std::ostringstream killOut;
    runLine(a.session.shell(), "KILLSESSION " + std::to_string(portB), killOut);
    CHECK(killOut.str() == "Session " + std::to_string(portB) + " killed\n");
    CHECK(host.sessions().find(portB) == nullptr);
    CHECK(host.sessions().count() == 1);

    std::ostringstream listOut;
    runLine(a.session.shell(), "LISTSESSIONS", listOut);
    CHECK(listOut.str().find(std::to_string(portB) + " ") == std::string::npos);

    std::ostringstream readOut;
    runLine(a.session.shell(), "READU DATA R1", readOut);
    CHECK(readOut.str() == "seed\n");

    const PickShell::SessionHandle reused = host.createSession();
    CHECK(reused.id == portB);
}

TEST_CASE("KILLSESSION rejects self, unknown port, and non-SYSPROG") {
    PickCore::GeminiServiceDaemon daemon = makeDaemon(2);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::GeminiSessionHost host(daemon, 2);
    const PickShell::SessionHandle a = host.createSession();
    const PickShell::SessionHandle b = host.createSession();
    a.session.attach(makeUser("SYSPROG", "SYSPROG", 1));
    b.session.attach(makeUser("TST", "TST", 2));
    installHostQueries(host, a.session);
    installHostQueries(host, b.session);

    std::ostringstream selfOut;
    runLine(a.session.shell(), "KILLSESSION " + std::to_string(a.id), selfOut);
    CHECK(selfOut.str() == "KILLSESSION: cannot kill current session\n");

    std::ostringstream missingOut;
    runLine(a.session.shell(), "KILLSESSION 99", missingOut);
    CHECK(missingOut.str() == "KILLSESSION: session not found\n");

    std::ostringstream denied;
    runLine(b.session.shell(), "KILLSESSION " + std::to_string(a.id), denied);
    CHECK(denied.str() == "KILLSESSION requires SYSPROG\n");
}

TEST_CASE("SHUTDOWN and SYSTEM SHUTDOWN invoke callback and set quit") {
    PickCore::GeminiServiceDaemon daemon = makeDaemon(1);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::GeminiSessionHost host(daemon, 1);
    const PickShell::SessionHandle session = host.createSession();
    session.session.attach(makeUser("SYSPROG", "SYSPROG", 1));

    std::atomic<int> shutdownCalls{0};
    session.session.shell().setAdminQueries(PickShell::ShellAdminQueries{
        .listSessions = [&host] { return host.listAdminSessions(); },
        .status = [&host] { return host.adminStatus(); },
        .killSession = nullptr,
        .requestShutdown = [&shutdownCalls] { shutdownCalls.fetch_add(1); },
    });

    std::ostringstream out;
    CHECK(runLineQuit(session.session.shell(), "SHUTDOWN", out));
    CHECK(out.str() == "Shutting down\n");
    CHECK(shutdownCalls.load() == 1);

    out.str("");
    CHECK(runLineQuit(session.session.shell(), "SYSTEM SHUTDOWN", out));
    CHECK(out.str() == "Shutting down\n");
    CHECK(shutdownCalls.load() == 2);

    PickShell::GeminiSession bare;
    bare.attach(makeUser("SYSPROG", "allan", 0));
    out.str("");
    CHECK_FALSE(runLineQuit(bare.shell(), "SHUTDOWN", out));
    CHECK(out.str() == "SHUTDOWN: not available\n");
}

TEST_CASE("KILLSESSION and SHUTDOWN require SYSPROG when logged out") {
    PickShell::GeminiSession gs;
    std::ostringstream out;
    runLine(gs.shell(), "KILLSESSION 1", out);
    CHECK(out.str() == "KILLSESSION requires SYSPROG\n");

    out.str("");
    runLine(gs.shell(), "SHUTDOWN", out);
    CHECK(out.str() == "SHUTDOWN requires SYSPROG\n");
}
