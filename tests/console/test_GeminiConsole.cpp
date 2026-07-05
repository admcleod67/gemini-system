#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>

#include "ConsoleConfig.h"
#include "DaemonIpcClient.h"

#ifndef _WIN32
    #include <filesystem>
    #include <fstream>
    #include <unistd.h>

    #include "DaemonConfig.h"
    #include "DaemonIpcProtocol.h"
    #include "DaemonIpcTestUtil.h"
    #include "GeminiDaemonRunner.h"
    #include "GeminiServiceDaemon.h"
    #include "GeminiSessionHost.h"

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
    firstClient.detachSession();
    firstClient.disconnect();

    PickCore::DaemonIpcClient secondClient(config.socketPath);
    secondClient.connect();
    secondClient.handshake();
    CHECK(secondClient.attachSession(port) == port);
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
    const PickCore::SessionId port = client.attachSession(0);

    std::atomic<bool> echoReady{false};
    std::thread echoThread([&] {
        host.runExclusive(port, [&] {
            PickShell::GeminiSession *session = host.sessions().find(port);
            REQUIRE(session != nullptr);
            echoReady.store(true);
            const int value = session->input().get();
            session->output().put(static_cast<char>(value));
            session->output().flush();
        });
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (!echoReady.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(echoReady.load());

    std::istringstream input("Z");
    std::ostringstream output;
    std::ostringstream diagnostic;

    std::thread pumpThread([&] { CHECK(client.runIoPump(input, output, diagnostic) == 0); });
    pumpThread.join();
    echoThread.join();

    CHECK(output.str() == "Z");

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

    std::istringstream loginInput("TST\n");
    std::ostringstream loginOutput;
    std::ostringstream loginDiagnostic;
    std::thread loginPump([&] { CHECK(firstClient.runIoPump(loginInput, loginOutput, loginDiagnostic) == 0); });
    loginPump.join();
    REQUIRE(waitForLoggedIn(host, port, std::chrono::milliseconds(2000)));

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

    secondClient.disconnect();
    runner.requestShutdown();
    runnerThread.join();
}

#endif
