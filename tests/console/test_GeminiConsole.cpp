#include <doctest/doctest.h>

#include <chrono>
#include <sstream>
#include <string>
#include <thread>

#include "ConsoleConfig.h"
#include "DaemonIpcClient.h"

#ifndef _WIN32
    #include <csignal>
    #include <filesystem>
    #include <fstream>
    #include <unistd.h>

    #include "DaemonConfig.h"
    #include "DaemonIpcProtocol.h"
    #include "DaemonIpcTestUtil.h"
    #include "GeminiDaemonRunner.h"
    #include "GeminiServiceDaemon.h"
    #include "GeminiSessionHost.h"

namespace {
    struct IgnoreSigPipe {
        IgnoreSigPipe() { std::signal(SIGPIPE, SIG_IGN); }
    };
    const IgnoreSigPipe kIgnoreSigPipe{};
} // namespace

using PickTests::connectUnixSocket;
using PickTests::recvFrame;
using PickTests::sendFrame;
using PickTests::sendHandshake;
using PickTests::uniqueDaemonIpcTempDir;
using PickTests::waitForSocket;

namespace {
    PickCore::DaemonConfig makeCatalogDaemonConfig() {
        const auto root = uniqueDaemonIpcTempDir();
        const auto gem = root / "gemini";
        const auto pick = gem / "accounts" / "TST";
        std::filesystem::create_directories(pick / "VOC");
        {
            std::ofstream accounts(gem / "ACCOUNTS.json");
            accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"}]})";
        }

        PickCore::DaemonConfig config{};
        config.maxSessions = 2;
        config.socketPath = root / "gemini.sock";
        config.hostPaths.geminiCatalogRoot = gem;
        config.hostPaths.pickFilesystemRoot = pick;
        return config;
    }

    bool waitForLoggedIn(PickShell::GeminiSessionHost &host,
                         const PickCore::SessionId port,
                         const std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            PickShell::GeminiSession *session = host.sessions().find(port);
            if (session != nullptr && session->loggedIn()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    std::string runIoPumpWithInput(PickCore::DaemonIpcClient &client, const std::string &input) {
        std::istringstream in(input);
        std::ostringstream output;
        std::ostringstream diagnostic;
        CHECK(client.runIoPump(in, output, diagnostic) == 0);
        return output.str();
    }
} // namespace

TEST_CASE("ConsoleConfig resolves defaults and flags") {
    const char *argv[] = {"gemini-console", "--socket", "/tmp/test.sock", "--port", "7"};
    const PickCore::ConsoleConfigResolution resolution =
        PickCore::resolveConsoleConfig(static_cast<int>(std::size(argv)), const_cast<char **>(argv));
    CHECK_FALSE(resolution.showHelp);
    CHECK(resolution.config.socketPath == "/tmp/test.sock");
    CHECK(resolution.config.requestedPort == 7);
}

TEST_CASE("ConsoleConfig --help") {
    const char *argv[] = {"gemini-console", "--help"};
    const PickCore::ConsoleConfigResolution resolution =
        PickCore::resolveConsoleConfig(static_cast<int>(std::size(argv)), const_cast<char **>(argv));
    CHECK(resolution.showHelp);
}

TEST_CASE("DaemonIpcClient connect fails on missing socket") {
    PickCore::DaemonIpcClient client(uniqueDaemonIpcTempDir() / "missing.sock");
    CHECK_THROWS_AS(client.connect(), PickCore::DaemonIpcClientError);
}

TEST_CASE("DaemonIpcClient attach unknown port returns SessionNotFound") {
    PickCore::DaemonConfig config{};
    config.maxSessions = 2;
    config.socketPath = uniqueDaemonIpcTempDir() / "gemini.sock";
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    PickCore::DaemonIpcClient client(config.socketPath);
    client.connect();
    client.handshake();
    CHECK_THROWS_WITH(client.attachSession(999), "session not found");

    client.disconnect();
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient re-attaches to detached session port") {
    PickCore::DaemonConfig config{};
    config.maxSessions = 2;
    config.socketPath = uniqueDaemonIpcTempDir() / "gemini.sock";
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    PickCore::DaemonIpcClient firstClient(config.socketPath);
    firstClient.connect();
    firstClient.handshake();
    const PickCore::SessionId port = firstClient.attachSession(0);
    (void) runIoPumpWithInput(firstClient, "QUIT\n");
    firstClient.detachSession();
    firstClient.disconnect();

    PickCore::DaemonIpcClient secondClient(config.socketPath);
    secondClient.connect();
    secondClient.handshake();
    CHECK(secondClient.attachSession(port) == port);
    (void) runIoPumpWithInput(secondClient, "QUIT\n");
    secondClient.detachSession();
    secondClient.disconnect();

    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient runIoPump round-trips bytes over IPC") {
    PickCore::DaemonConfig config{};
    config.maxSessions = 2;
    config.socketPath = uniqueDaemonIpcTempDir() / "gemini.sock";
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    PickCore::DaemonIpcClient client(config.socketPath);
    client.connect();
    client.handshake();
    (void) client.attachSession(0);

    const std::string output = runIoPumpWithInput(client, "WHO\nQUIT\n");

    CHECK(output.find("Gemini/TCL Developer Shell") != std::string::npos);
    CHECK(output.find("0 - -\n") != std::string::npos);

    client.disconnect();
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient completes catalogue login over IPC") {
    const PickCore::DaemonConfig config = makeCatalogDaemonConfig();
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    PickCore::DaemonIpcClient client(config.socketPath);
    client.connect();
    client.handshake();
    const PickCore::SessionId port = client.attachSession(0);

    std::istringstream input("TST\n");
    std::ostringstream output;
    std::ostringstream diagnostic;

    std::thread pumpThread([&] { CHECK(client.runIoPump(input, output, diagnostic) == 0); });
    pumpThread.join();

    REQUIRE(waitForLoggedIn(host, port, std::chrono::milliseconds(2000)));

    PickShell::GeminiSession *session = host.sessions().find(port);
    REQUIRE(session != nullptr);
    CHECK(session->sessionAccount() == "TST");
    CHECK(session->sessionUsername() == "TST");
    CHECK(output.str().find("LOGON PLEASE:") != std::string::npos);

    client.disconnect();
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient re-attaches to logged-in session without re-login") {
    const PickCore::DaemonConfig config = makeCatalogDaemonConfig();
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    PickCore::DaemonIpcClient firstClient(config.socketPath);
    firstClient.connect();
    firstClient.handshake();
    const PickCore::SessionId port = firstClient.attachSession(0);

    const std::string loginOutput = runIoPumpWithInput(firstClient, "TST\n");
    REQUIRE(waitForLoggedIn(host, port, std::chrono::milliseconds(2000)));
    CHECK(loginOutput.find("LOGON PLEASE:") != std::string::npos);

    firstClient.detachSession();
    firstClient.disconnect();

    PickCore::DaemonIpcClient secondClient(config.socketPath);
    secondClient.connect();
    secondClient.handshake();
    CHECK(secondClient.attachSession(port) == port);

    PickShell::GeminiSession *session = host.sessions().find(port);
    REQUIRE(session != nullptr);
    CHECK(session->loggedIn());
    CHECK(session->sessionAccount() == "TST");

    (void) runIoPumpWithInput(secondClient, "QUIT\n");
    secondClient.detachSession();
    secondClient.disconnect();
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient runs WHO over IPC after catalogue login") {
    const PickCore::DaemonConfig config = makeCatalogDaemonConfig();
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    PickCore::DaemonIpcClient client(config.socketPath);
    client.connect();
    client.handshake();
    const PickCore::SessionId port = client.attachSession(0);

    const std::string output = runIoPumpWithInput(client, "TST\nWHO\nQUIT\n");

    PickShell::GeminiSession *session = host.sessions().find(port);
    REQUIRE(session != nullptr);
    const std::string whoLine = std::to_string(session->whoPort()) + " TST TST\n";

    CHECK(output.find("LOGON PLEASE:") != std::string::npos);
    CHECK(output.find("Gemini/TCL Developer Shell") != std::string::npos);
    CHECK(output.find(whoLine) != std::string::npos);

    client.disconnect();
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient two consoles receive distinct WHO ports") {
    const PickCore::DaemonConfig config = makeCatalogDaemonConfig();
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    PickCore::DaemonIpcClient clientA(config.socketPath);
    clientA.connect();
    clientA.handshake();
    const PickCore::SessionId portA = clientA.attachSession(0);

    PickCore::DaemonIpcClient clientB(config.socketPath);
    clientB.connect();
    clientB.handshake();
    const PickCore::SessionId portB = clientB.attachSession(0);
    REQUIRE(portA != portB);

    const std::string outputA = runIoPumpWithInput(clientA, "TST\nWHO\nQUIT\n");
    const std::string outputB = runIoPumpWithInput(clientB, "TST\nWHO\nQUIT\n");

    PickShell::GeminiSession *sessionA = host.sessions().find(portA);
    PickShell::GeminiSession *sessionB = host.sessions().find(portB);
    REQUIRE(sessionA != nullptr);
    REQUIRE(sessionB != nullptr);

    const std::string whoLineA = std::to_string(sessionA->whoPort()) + " TST TST\n";
    const std::string whoLineB = std::to_string(sessionB->whoPort()) + " TST TST\n";
    CHECK(sessionA->whoPort() != sessionB->whoPort());
    CHECK(outputA.find(whoLineA) != std::string::npos);
    CHECK(outputB.find(whoLineB) != std::string::npos);

    clientA.disconnect();
    clientB.disconnect();
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient detach and re-attach preserves WHO without re-login") {
    const PickCore::DaemonConfig config = makeCatalogDaemonConfig();
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    PickCore::DaemonIpcClient firstClient(config.socketPath);
    firstClient.connect();
    firstClient.handshake();
    const PickCore::SessionId port = firstClient.attachSession(0);

    const std::string loginOutput = runIoPumpWithInput(firstClient, "TST\nWHO\n");
    REQUIRE(waitForLoggedIn(host, port, std::chrono::milliseconds(2000)));

    PickShell::GeminiSession *session = host.sessions().find(port);
    REQUIRE(session != nullptr);
    const int whoPort = session->whoPort();
    const std::string whoLine = std::to_string(whoPort) + " TST TST\n";
    CHECK(loginOutput.find(whoLine) != std::string::npos);

    firstClient.detachSession();
    firstClient.disconnect();

    REQUIRE(session->loggedIn());
    CHECK(session->whoPort() == whoPort);

    PickCore::DaemonIpcClient secondClient(config.socketPath);
    secondClient.connect();
    secondClient.handshake();
    CHECK(secondClient.attachSession(port) == port);

    const std::string reattachOutput = runIoPumpWithInput(secondClient, "WHO\nQUIT\n");
    CHECK(reattachOutput.find("LOGON PLEASE:") == std::string::npos);
    CHECK(reattachOutput.find(whoLine) != std::string::npos);

    secondClient.disconnect();
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient LOGOFF while attached returns to login prompt") {
    const PickCore::DaemonConfig config = makeCatalogDaemonConfig();
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    PickCore::DaemonIpcClient client(config.socketPath);
    client.connect();
    client.handshake();
    const PickCore::SessionId port = client.attachSession(0);

    const std::string output = runIoPumpWithInput(client, "TST\nLOGOFF\nQUIT\n");

    CHECK(output.find("Logged off.") != std::string::npos);
    CHECK(output.rfind("LOGON PLEASE:") > output.find("Logged off."));

    PickShell::GeminiSession *session = host.sessions().find(port);
    REQUIRE(session != nullptr);
    CHECK_FALSE(session->loggedIn());

    client.disconnect();
    runner.requestShutdown();
    runnerThread.join();
}

#endif
