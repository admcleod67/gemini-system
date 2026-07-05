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

#endif
