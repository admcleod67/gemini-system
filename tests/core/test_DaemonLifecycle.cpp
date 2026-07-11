#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include "DaemonConfig.h"
#include "GeminiDaemonRunner.h"
#include "GeminiServiceDaemon.h"
#include "GeminiSessionHost.h"

static std::filesystem::path uniqueTempDir() {
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("pick-daemon-lifecycle-" + std::to_string(tick));
}

TEST_CASE("GeminiServiceDaemon create uses config maxSessions for port manager") {
    PickCore::DaemonConfig config{};
    config.maxSessions = 4;

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    CHECK(daemon.maxSessions() == 4);
    CHECK(daemon.portManager().maxPorts() == 4);
}

TEST_CASE("GeminiServiceDaemon embedded coordinates table and port capacity") {
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::createEmbedded();
    CHECK(daemon.maxSessions() == 1);
    CHECK(daemon.portManager().maxPorts() == 1);
}

TEST_CASE("GeminiDaemonRunner shutdown destroys sessions and releases ports") {
    const auto root = uniqueTempDir();
    std::filesystem::create_directories(root);

    PickCore::DaemonConfig config{};
    config.maxSessions = 2;
    config.socketPath = root / "gemini.sock";

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::ostringstream boot;
    daemon.coldStart(boot);

    (void) host.createSession();
    (void) host.createSession();
    CHECK(host.sessions().count() == 2);
    CHECK(daemon.portManager().inUseCount() == 2);

    std::thread runnerThread([&] {
        std::ostringstream idleBoot;
        CHECK(runner.run(idleBoot) == 0);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    runner.requestShutdown();
    runnerThread.join();

    CHECK(host.sessions().count() == 0);
    CHECK(daemon.portManager().inUseCount() == 0);
}

TEST_CASE("GeminiDaemonRunner shutdown unlinks configured socket file") {
    const auto root = uniqueTempDir();
    std::filesystem::create_directories(root);
    const std::filesystem::path socketPath = root / "gemini.sock";
    {
        std::ofstream touch(socketPath);
        touch << "placeholder";
    }
    REQUIRE(std::filesystem::exists(socketPath));

    PickCore::DaemonConfig config{};
    config.maxSessions = 1;
    config.socketPath = socketPath;

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    runner.requestShutdown();
    runnerThread.join();

    CHECK_FALSE(std::filesystem::exists(socketPath));
}

TEST_CASE("GeminiDaemonRunner bootOut includes banner MODULES SYSTEM READY and IPC LISTENING") {
#ifndef _WIN32
    const auto root = uniqueTempDir();
    std::filesystem::create_directories(root);
    const std::filesystem::path socketPath = root / "gemini.sock";

    PickCore::DaemonConfig config{};
    config.maxSessions = 1;
    config.socketPath = socketPath;

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::ostringstream boot;
    std::thread runnerThread([&] { CHECK(runner.run(boot) == 0); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    runner.requestShutdown();
    runnerThread.join();

    const std::string text = boot.str();
    CHECK(text.find("INITIALIZING SYSTEM") != std::string::npos);
    CHECK(text.find("MODULES:") != std::string::npos);
    CHECK(text.find("SYSTEM READY") != std::string::npos);
    CHECK(text.find("IPC LISTENING: " + socketPath.string()) != std::string::npos);
#endif
}

TEST_CASE("GeminiSessionHost destroyAllSessions clears table") {
    PickCore::DaemonConfig config{};
    config.maxSessions = 2;
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::GeminiSessionHost host(daemon, 2);
    (void) host.createSession();
    (void) host.createSession();
    CHECK(host.sessions().count() == 2);

    host.destroyAllSessions();
    CHECK(host.sessions().count() == 0);
    CHECK(daemon.portManager().inUseCount() == 0);
}
