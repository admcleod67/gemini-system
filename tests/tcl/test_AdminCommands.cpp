#include <doctest/doctest.h>

#include <filesystem>
#include <sstream>
#include <string>

#include <pick_system/version.hpp>

#include "DaemonConfig.h"
#include "GeminiServiceDaemon.h"
#include "GeminiSession.h"
#include "GeminiSessionHost.h"
#include "Shell.h"
#include "UserSession.h"

namespace {
    [[nodiscard]] PickCore::GeminiServiceDaemon makeDaemon(const std::size_t maxSessions) {
        PickCore::DaemonConfig config{};
        config.maxSessions = maxSessions;
        return PickCore::GeminiServiceDaemon::create(config);
    }

    [[nodiscard]] PickCore::UserSession makeUser(const std::string &account,
                                                 const std::string &username,
                                                 const int port) {
        PickCore::UserSession user;
        user.accountName = account;
        user.username = username;
        user.whoPort = port;
        user.userNo = std::to_string(port);
        user.pickRoot = std::filesystem::temp_directory_path();
        return user;
    }

    void installHostQueries(PickShell::GeminiSessionHost &host, PickShell::GeminiSession &session) {
        session.shell().setAdminQueries(PickShell::ShellAdminQueries{
            [&host] { return host.listAdminSessions(); },
            [&host] { return host.adminStatus(); },
        });
    }

    void runLine(PickShell::Shell &shell, const std::string &line, std::ostringstream &out) {
        bool quit = false;
        shell.handleLine(line, out, quit);
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
