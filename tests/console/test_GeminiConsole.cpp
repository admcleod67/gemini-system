#include <doctest/doctest.h>

#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ConsoleConfig.h"
#include "DaemonIpcClient.h"

#ifndef _WIN32
    #include <csignal>
    #include <filesystem>
    #include <fstream>
    #include <poll.h>
    #include <unistd.h>

    #include "DaemonConfig.h"
    #include "DaemonIpcProtocol.h"
    #include "DaemonIpcTestUtil.h"
    #include "FileSystem.h"
    #include "GeminiDaemonRunner.h"
    #include "GeminiServiceDaemon.h"
    #include "GeminiSessionHost.h"

    #include <pick_system/version.hpp>

namespace {
    struct IgnoreSigPipe {
        IgnoreSigPipe() { std::signal(SIGPIPE, SIG_IGN); }
    };
    const IgnoreSigPipe kIgnoreSigPipe{};
} // namespace

using PickTests::connectUnixSocket;
using PickTests::recvFrame;
using PickTests::recvHandshakeAck;
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

    PickCore::DaemonConfig makeCatalogDaemonConfigThreeSession() {
        PickCore::DaemonConfig config = makeCatalogDaemonConfig();
        config.maxSessions = 3;
        return config;
    }

    void seedVocAndBp(const std::filesystem::path &fsRoot) {
        PickFS::FileSystem fs(fsRoot);
        fs.createFile("VOC");
        fs.createFile("BP");
        fs.write("VOC", PickFS::Record("BP", "001 F\n002 BP\n003 /gemini/fs/DM/BP\n"));
    }

    void writeProgramSourceRecord(const std::filesystem::path &fsRoot,
                                  const std::string &programName,
                                  const std::string &source) {
        PickFS::FileSystem fs(fsRoot);
        fs.write("BP", PickFS::Record(programName, source));
    }

    PickCore::DaemonConfig makeCatalogDaemonConfigWithInputProgram() {
        const auto root = uniqueDaemonIpcTempDir();
        const auto gem = root / "gemini";
        const auto pick = gem / "accounts" / "TST";
        std::filesystem::create_directories(pick);
        {
            std::ofstream accounts(gem / "ACCOUNTS.json");
            accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"}]})";
        }
        seedVocAndBp(pick);
        writeProgramSourceRecord(pick, "INPUTWAIT", "10 INPUT A\n20 END\n");

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

    PickCore::SessionId attachNewSession(const int clientFd) {
        PickCore::DaemonIpcAttachSessionPayload attachPayload{};
        attachPayload.requestedPort = 0;
        PickCore::DaemonIpcFrame attach{};
        attach.type = PickCore::DaemonIpcMessageType::AttachSession;
        attach.payload = PickCore::encodeAttachSessionPayload(attachPayload);
        sendFrame(clientFd, attach);

        const PickCore::DaemonIpcFrame ack = recvFrame(clientFd);
        REQUIRE(ack.type == PickCore::DaemonIpcMessageType::AttachSessionAck);
        const std::optional<PickCore::DaemonIpcAttachSessionAckPayload> ackPayload =
            PickCore::decodeAttachSessionAckPayload(ack.payload);
        REQUIRE(ackPayload.has_value());
        return ackPayload->sessionPort;
    }

    void sendSessionInput(const int clientFd, const std::vector<std::uint8_t> &bytes) {
        PickCore::DaemonIpcSessionDataPayload payload{};
        payload.data = bytes;
        PickCore::DaemonIpcFrame frame{};
        frame.type = PickCore::DaemonIpcMessageType::SessionInput;
        frame.payload = PickCore::encodeSessionDataPayload(payload);
        sendFrame(clientFd, frame);
    }

    void sendSessionInputString(const int clientFd, const std::string &text) {
        sendSessionInput(clientFd,
                         std::vector<std::uint8_t>(reinterpret_cast<const std::uint8_t *>(text.data()),
                                                   reinterpret_cast<const std::uint8_t *>(text.data() + text.size())));
    }

    bool waitForSessionRunState(PickShell::GeminiSessionHost &host,
                                const PickCore::SessionId port,
                                const PickCore::SessionRunState expected,
                                const std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (host.sessionRunState(port) == expected) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return host.sessionRunState(port) == expected;
    }

    bool waitForSessionOutputContaining(const int clientFd,
                                        const std::string &needle,
                                        std::string &accumulated,
                                        const std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (accumulated.find(needle) != std::string::npos) {
                return true;
            }

            pollfd pfd{};
            pfd.fd = clientFd;
            pfd.events = POLLIN;
            const int pollResult = ::poll(&pfd, 1, 50);
            if (pollResult <= 0 || (pfd.revents & POLLIN) == 0) {
                continue;
            }

            const PickCore::DaemonIpcFrame frame = recvFrame(clientFd);
            if (frame.type != PickCore::DaemonIpcMessageType::SessionOutput) {
                continue;
            }
            const std::optional<PickCore::DaemonIpcSessionDataPayload> payload =
                PickCore::decodeSessionDataPayload(frame.payload);
            if (!payload.has_value()) {
                continue;
            }
            accumulated.append(reinterpret_cast<const char *>(payload->data.data()), payload->data.size());
        }
        return accumulated.find(needle) != std::string::npos;
    }

    bool loginAndWaitForTclPrompt(const int clientFd,
                                  PickShell::GeminiSessionHost &host,
                                  const PickCore::SessionId port,
                                  std::string &output) {
        sendSessionInputString(clientFd, "TST\n");
        if (!waitForSessionOutputContaining(clientFd, "TCL> ", output, std::chrono::milliseconds(2000))) {
            return false;
        }
        return waitForLoggedIn(host, port, std::chrono::milliseconds(2000));
    }

    void recvDetachSessionAckSkippingSessionIo(const int clientFd) {
        for (;;) {
            const PickCore::DaemonIpcFrame frame = recvFrame(clientFd);
            if (frame.type == PickCore::DaemonIpcMessageType::DetachSessionAck) {
                return;
            }
            REQUIRE((frame.type == PickCore::DaemonIpcMessageType::SessionOutput ||
                     frame.type == PickCore::DaemonIpcMessageType::SessionDiagnostic));
        }
    }

    void detachAndClose(const int clientFd) {
        PickCore::DaemonIpcFrame detach{};
        detach.type = PickCore::DaemonIpcMessageType::DetachSession;
        sendFrame(clientFd, detach);
        recvDetachSessionAckSkippingSessionIo(clientFd);
        ::close(clientFd);
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

    const std::string output = runIoPumpWithInput(client, "TST\n");
    REQUIRE(waitForLoggedIn(host, port, std::chrono::milliseconds(2000)));

    PickShell::GeminiSession *session = host.sessions().find(port);
    REQUIRE(session != nullptr);
    CHECK(session->sessionAccount() == "TST");
    CHECK(session->sessionUsername() == "TST");
    CHECK(output.find("LOGON PLEASE:") != std::string::npos);

    client.detachSession();
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

TEST_CASE("DaemonIpcClient two consoles display LOGON PLEASE concurrently") {
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

    const int clientFdA = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdA >= 0);
    sendHandshake(clientFdA);
    recvHandshakeAck(clientFdA);
    const PickCore::SessionId portA = attachNewSession(clientFdA);

    const int clientFdB = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdB >= 0);
    sendHandshake(clientFdB);
    recvHandshakeAck(clientFdB);
    const PickCore::SessionId portB = attachNewSession(clientFdB);
    REQUIRE(portA != portB);

    std::string outputA;
    std::string outputB;
    REQUIRE(waitForSessionOutputContaining(clientFdA, "LOGON PLEASE:", outputA,
                                           std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdB, "LOGON PLEASE:", outputB,
                                           std::chrono::milliseconds(2000)));

    PickShell::GeminiSession *sessionA = host.sessions().find(portA);
    PickShell::GeminiSession *sessionB = host.sessions().find(portB);
    REQUIRE(sessionA != nullptr);
    REQUIRE(sessionB != nullptr);
    CHECK_FALSE(sessionA->loggedIn());
    CHECK_FALSE(sessionB->loggedIn());
    CHECK(host.sessionRunState(portA) == PickCore::SessionRunState::WaitingForInput);
    CHECK(host.sessionRunState(portB) == PickCore::SessionRunState::WaitingForInput);

    detachAndClose(clientFdA);
    detachAndClose(clientFdB);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient two consoles display TCL prompt concurrently") {
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

    const int clientFdA = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdA >= 0);
    sendHandshake(clientFdA);
    recvHandshakeAck(clientFdA);
    const PickCore::SessionId portA = attachNewSession(clientFdA);

    const int clientFdB = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdB >= 0);
    sendHandshake(clientFdB);
    recvHandshakeAck(clientFdB);
    const PickCore::SessionId portB = attachNewSession(clientFdB);
    REQUIRE(portA != portB);

    std::string outputA;
    std::string outputB;
    REQUIRE(waitForSessionOutputContaining(clientFdA, "LOGON PLEASE:", outputA,
                                           std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdB, "LOGON PLEASE:", outputB,
                                           std::chrono::milliseconds(2000)));

    sendSessionInputString(clientFdA, "TST\n");
    REQUIRE(waitForSessionOutputContaining(clientFdA, "TCL> ", outputA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionRunState(host, portA, PickCore::SessionRunState::WaitingForInput,
                                   std::chrono::milliseconds(2000)));

    PickShell::GeminiSession *sessionB = host.sessions().find(portB);
    REQUIRE(sessionB != nullptr);
    CHECK_FALSE(sessionB->loggedIn());
    CHECK(outputB.find("TCL> ") == std::string::npos);
    CHECK(host.sessionRunState(portB) == PickCore::SessionRunState::WaitingForInput);

    sendSessionInputString(clientFdB, "TST\n");
    REQUIRE(waitForSessionOutputContaining(clientFdB, "TCL> ", outputB, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portB, std::chrono::milliseconds(2000)));
    CHECK(host.sessionRunState(portA) == PickCore::SessionRunState::WaitingForInput);
    CHECK(host.sessionRunState(portB) == PickCore::SessionRunState::WaitingForInput);

    detachAndClose(clientFdA);
    detachAndClose(clientFdB);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient two consoles accept interleaved Tcl commands") {
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

    const int clientFdA = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdA >= 0);
    sendHandshake(clientFdA);
    recvHandshakeAck(clientFdA);
    const PickCore::SessionId portA = attachNewSession(clientFdA);

    const int clientFdB = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdB >= 0);
    sendHandshake(clientFdB);
    recvHandshakeAck(clientFdB);
    const PickCore::SessionId portB = attachNewSession(clientFdB);
    REQUIRE(portA != portB);

    std::string outputA;
    std::string outputB;
    sendSessionInputString(clientFdA, "TST\n");
    sendSessionInputString(clientFdB, "TST\n");
    REQUIRE(waitForSessionOutputContaining(clientFdA, "TCL> ", outputA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdB, "TCL> ", outputB, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portB, std::chrono::milliseconds(2000)));

    PickShell::GeminiSession *sessionA = host.sessions().find(portA);
    PickShell::GeminiSession *sessionB = host.sessions().find(portB);
    REQUIRE(sessionA != nullptr);
    REQUIRE(sessionB != nullptr);

    const std::string whoLineA = std::to_string(sessionA->whoPort()) + " TST TST\n";
    sendSessionInputString(clientFdA, "WHO\n");
    REQUIRE(waitForSessionOutputContaining(clientFdA, whoLineA, outputA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionRunState(host, portA, PickCore::SessionRunState::WaitingForInput,
                                   std::chrono::milliseconds(2000)));

    sendSessionInputString(clientFdB, "VERSION\n");
    REQUIRE(waitForSessionOutputContaining(clientFdB, pick_system::version_string, outputB,
                                           std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionRunState(host, portB, PickCore::SessionRunState::WaitingForInput,
                                   std::chrono::milliseconds(2000)));

    sendSessionInputString(clientFdA, "WHO\n");
    REQUIRE(waitForSessionOutputContaining(clientFdA, whoLineA, outputA, std::chrono::milliseconds(2000)));

    sendSessionInputString(clientFdA, "QUIT\n");
    sendSessionInputString(clientFdB, "QUIT\n");
    detachAndClose(clientFdA);
    detachAndClose(clientFdB);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient session blocked in BASIC INPUT allows other session Tcl WHO") {
    const PickCore::DaemonConfig config = makeCatalogDaemonConfigWithInputProgram();
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    const int clientFdA = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdA >= 0);
    sendHandshake(clientFdA);
    recvHandshakeAck(clientFdA);
    const PickCore::SessionId portA = attachNewSession(clientFdA);

    std::string outputA;
    REQUIRE(loginAndWaitForTclPrompt(clientFdA, host, portA, outputA));

    sendSessionInputString(clientFdA, "RUN INPUTWAIT\n");
    REQUIRE(waitForSessionRunState(host, portA, PickCore::SessionRunState::WaitingForInput,
                                   std::chrono::milliseconds(2000)));

    const int clientFdB = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdB >= 0);
    sendHandshake(clientFdB);
    recvHandshakeAck(clientFdB);
    const PickCore::SessionId portB = attachNewSession(clientFdB);
    REQUIRE(portA != portB);

    std::string outputB;
    REQUIRE(loginAndWaitForTclPrompt(clientFdB, host, portB, outputB));

    PickShell::GeminiSession *sessionA = host.sessions().find(portA);
    PickShell::GeminiSession *sessionB = host.sessions().find(portB);
    REQUIRE(sessionA != nullptr);
    REQUIRE(sessionB != nullptr);

    const std::string whoLineB = std::to_string(sessionB->whoPort()) + " TST TST\n";
    sendSessionInputString(clientFdB, "WHO\n");
    REQUIRE(waitForSessionOutputContaining(clientFdB, whoLineB, outputB, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionRunState(host, portB, PickCore::SessionRunState::WaitingForInput,
                                   std::chrono::milliseconds(2000)));
    CHECK(host.sessionRunState(portA) == PickCore::SessionRunState::WaitingForInput);

    sendSessionInputString(clientFdA, "42\n");
    sendSessionInputString(clientFdA, "QUIT\n");
    sendSessionInputString(clientFdB, "QUIT\n");
    detachAndClose(clientFdA);
    detachAndClose(clientFdB);
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

TEST_CASE("DaemonIpcClient three consoles display TCL prompt concurrently") {
    const PickCore::DaemonConfig config = makeCatalogDaemonConfigThreeSession();
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    const int clientFdA = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdA >= 0);
    sendHandshake(clientFdA);
    recvHandshakeAck(clientFdA);
    const PickCore::SessionId portA = attachNewSession(clientFdA);

    const int clientFdB = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdB >= 0);
    sendHandshake(clientFdB);
    recvHandshakeAck(clientFdB);
    const PickCore::SessionId portB = attachNewSession(clientFdB);

    const int clientFdC = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdC >= 0);
    sendHandshake(clientFdC);
    recvHandshakeAck(clientFdC);
    const PickCore::SessionId portC = attachNewSession(clientFdC);
    REQUIRE(portA != portB);
    REQUIRE(portA != portC);
    REQUIRE(portB != portC);

    std::string outputA;
    std::string outputB;
    std::string outputC;
    sendSessionInputString(clientFdA, "TST\n");
    sendSessionInputString(clientFdB, "TST\n");
    sendSessionInputString(clientFdC, "TST\n");
    REQUIRE(waitForSessionOutputContaining(clientFdA, "TCL> ", outputA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdB, "TCL> ", outputB, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdC, "TCL> ", outputC, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portB, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portC, std::chrono::milliseconds(2000)));
    CHECK(host.sessionRunState(portA) == PickCore::SessionRunState::WaitingForInput);
    CHECK(host.sessionRunState(portB) == PickCore::SessionRunState::WaitingForInput);
    CHECK(host.sessionRunState(portC) == PickCore::SessionRunState::WaitingForInput);

    detachAndClose(clientFdA);
    detachAndClose(clientFdB);
    detachAndClose(clientFdC);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient three consoles receive distinct WHO ports") {
    const PickCore::DaemonConfig config = makeCatalogDaemonConfigThreeSession();
    std::filesystem::create_directories(config.socketPath.parent_path());

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    const int clientFdA = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdA >= 0);
    sendHandshake(clientFdA);
    recvHandshakeAck(clientFdA);
    const PickCore::SessionId portA = attachNewSession(clientFdA);

    const int clientFdB = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdB >= 0);
    sendHandshake(clientFdB);
    recvHandshakeAck(clientFdB);
    const PickCore::SessionId portB = attachNewSession(clientFdB);

    const int clientFdC = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdC >= 0);
    sendHandshake(clientFdC);
    recvHandshakeAck(clientFdC);
    const PickCore::SessionId portC = attachNewSession(clientFdC);
    REQUIRE(portA != portB);
    REQUIRE(portA != portC);
    REQUIRE(portB != portC);

    std::string outputA;
    std::string outputB;
    std::string outputC;
    sendSessionInputString(clientFdA, "TST\n");
    sendSessionInputString(clientFdB, "TST\n");
    sendSessionInputString(clientFdC, "TST\n");
    REQUIRE(waitForSessionOutputContaining(clientFdA, "TCL> ", outputA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdB, "TCL> ", outputB, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdC, "TCL> ", outputC, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portB, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portC, std::chrono::milliseconds(2000)));

    PickShell::GeminiSession *sessionA = host.sessions().find(portA);
    PickShell::GeminiSession *sessionB = host.sessions().find(portB);
    PickShell::GeminiSession *sessionC = host.sessions().find(portC);
    REQUIRE(sessionA != nullptr);
    REQUIRE(sessionB != nullptr);
    REQUIRE(sessionC != nullptr);

    const std::string whoLineA = std::to_string(sessionA->whoPort()) + " TST TST\n";
    const std::string whoLineB = std::to_string(sessionB->whoPort()) + " TST TST\n";
    const std::string whoLineC = std::to_string(sessionC->whoPort()) + " TST TST\n";
    CHECK(sessionA->whoPort() != sessionB->whoPort());
    CHECK(sessionA->whoPort() != sessionC->whoPort());
    CHECK(sessionB->whoPort() != sessionC->whoPort());

    sendSessionInputString(clientFdA, "WHO\n");
    REQUIRE(waitForSessionOutputContaining(clientFdA, whoLineA, outputA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionRunState(host, portA, PickCore::SessionRunState::WaitingForInput,
                                   std::chrono::milliseconds(2000)));

    sendSessionInputString(clientFdB, "WHO\n");
    REQUIRE(waitForSessionOutputContaining(clientFdB, whoLineB, outputB, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionRunState(host, portB, PickCore::SessionRunState::WaitingForInput,
                                   std::chrono::milliseconds(2000)));

    sendSessionInputString(clientFdC, "WHO\n");
    REQUIRE(waitForSessionOutputContaining(clientFdC, whoLineC, outputC, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionRunState(host, portC, PickCore::SessionRunState::WaitingForInput,
                                   std::chrono::milliseconds(2000)));

    sendSessionInputString(clientFdA, "QUIT\n");
    sendSessionInputString(clientFdB, "QUIT\n");
    sendSessionInputString(clientFdC, "QUIT\n");
    detachAndClose(clientFdA);
    detachAndClose(clientFdB);
    detachAndClose(clientFdC);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("DaemonIpcClient SYSPROG LISTSESSIONS and STATUS show both consoles") {
    const auto root = uniqueDaemonIpcTempDir();
    const auto gem = root / "gemini";
    const auto sysprog = gem / "accounts" / "SYSPROG";
    const auto tst = gem / "accounts" / "TST";
    std::filesystem::create_directories(sysprog / "VOC");
    std::filesystem::create_directories(tst / "VOC");
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"SYSPROG","root":"accounts/SYSPROG"},{"name":"TST","root":"accounts/TST"}]})";
    }

    PickCore::DaemonConfig config{};
    config.maxSessions = 2;
    config.socketPath = root / "gemini.sock";
    config.hostPaths.geminiCatalogRoot = gem;
    config.hostPaths.pickFilesystemRoot = sysprog;

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    PickShell::GeminiSessionHost host(daemon, config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, config);

    std::thread runnerThread([&] {
        std::ostringstream boot;
        CHECK(runner.run(boot) == 0);
    });

    REQUIRE(waitForSocket(config.socketPath, std::chrono::milliseconds(2000)));

    const int clientFdA = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdA >= 0);
    sendHandshake(clientFdA);
    recvHandshakeAck(clientFdA);
    const PickCore::SessionId portA = attachNewSession(clientFdA);

    const int clientFdB = connectUnixSocket(config.socketPath);
    REQUIRE(clientFdB >= 0);
    sendHandshake(clientFdB);
    recvHandshakeAck(clientFdB);
    const PickCore::SessionId portB = attachNewSession(clientFdB);
    REQUIRE(portA != portB);

    std::string outputA;
    std::string outputB;
    sendSessionInputString(clientFdA, "SYSPROG\n");
    sendSessionInputString(clientFdB, "TST\n");
    REQUIRE(waitForSessionOutputContaining(clientFdA, "TCL> ", outputA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdB, "TCL> ", outputB, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portA, std::chrono::milliseconds(2000)));
    REQUIRE(waitForLoggedIn(host, portB, std::chrono::milliseconds(2000)));

    sendSessionInputString(clientFdA, "LISTSESSIONS\n");
    REQUIRE(waitForSessionOutputContaining(clientFdA, "PORT BOUND USER ACCOUNT STATE\n", outputA,
                                           std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdA, std::to_string(portA) + " yes SYSPROG SYSPROG", outputA,
                                           std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdA, std::to_string(portB) + " yes TST TST", outputA,
                                           std::chrono::milliseconds(2000)));

    sendSessionInputString(clientFdA, "STATUS\n");
    REQUIRE(waitForSessionOutputContaining(clientFdA, "sessions=2 maxSessions=2\n", outputA,
                                           std::chrono::milliseconds(2000)));
    REQUIRE(waitForSessionOutputContaining(clientFdA, "socket=" + config.socketPath.string() + "\n", outputA,
                                           std::chrono::milliseconds(2000)));

    sendSessionInputString(clientFdA, "QUIT\n");
    sendSessionInputString(clientFdB, "QUIT\n");
    detachAndClose(clientFdA);
    detachAndClose(clientFdB);
    runner.requestShutdown();
    runnerThread.join();
}

#endif
