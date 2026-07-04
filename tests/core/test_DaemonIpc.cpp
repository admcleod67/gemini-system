#include <doctest/doctest.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "DaemonConfig.h"
#include "DaemonIpcProtocol.h"
#include "DaemonIpcTestUtil.h"
#include "GeminiDaemonRunner.h"
#include "GeminiServiceDaemon.h"
#include "GeminiSessionHost.h"

#ifndef _WIN32
    #include <sys/stat.h>
#endif

using PickTests::connectUnixSocket;
using PickTests::recvFrame;
using PickTests::sendFrame;
using PickTests::uniqueDaemonIpcTempDir;
using PickTests::waitForSocket;

TEST_CASE("DaemonIpcProtocol encodes and decodes handshake frame") {
    PickCore::DaemonIpcFrame frame{};
    frame.type = PickCore::DaemonIpcMessageType::Handshake;
    frame.payload = PickCore::encodeHandshakePayload(PickCore::DaemonIpcHandshakePayload{1});

    const std::vector<std::uint8_t> bytes = PickCore::encodeDaemonIpcFrame(frame);
    const std::optional<PickCore::DaemonIpcFrame> decoded = PickCore::decodeDaemonIpcFrame(bytes);
    REQUIRE(decoded.has_value());
    CHECK(decoded->version == 1);
    CHECK(decoded->type == PickCore::DaemonIpcMessageType::Handshake);
    const std::optional<PickCore::DaemonIpcHandshakePayload> payload =
        PickCore::decodeHandshakePayload(decoded->payload);
    REQUIRE(payload.has_value());
    CHECK(payload->clientProtocolVersion == 1);
}

TEST_CASE("DaemonIpcProtocol encodes and decodes handshake ack frame") {
    PickCore::DaemonIpcHandshakeAckPayload ack{};
    ack.serverProtocolVersion = 1;
    ack.maxSessions = 8;
    ack.buildVersion = "0.13.0";

    PickCore::DaemonIpcFrame frame{};
    frame.type = PickCore::DaemonIpcMessageType::HandshakeAck;
    frame.payload = PickCore::encodeHandshakeAckPayload(ack);

    const std::optional<PickCore::DaemonIpcFrame> decoded = PickCore::decodeDaemonIpcFrame(PickCore::encodeDaemonIpcFrame(frame));
    REQUIRE(decoded.has_value());
    const std::optional<PickCore::DaemonIpcHandshakeAckPayload> payload =
        PickCore::decodeHandshakeAckPayload(decoded->payload);
    REQUIRE(payload.has_value());
    CHECK(payload->serverProtocolVersion == 1);
    CHECK(payload->maxSessions == 8);
    CHECK(payload->buildVersion == "0.13.0");
}

TEST_CASE("DaemonIpcProtocol encodes and decodes ping and pong frames") {
    PickCore::DaemonIpcFrame ping{};
    ping.type = PickCore::DaemonIpcMessageType::Ping;
    const std::optional<PickCore::DaemonIpcFrame> decodedPing =
        PickCore::decodeDaemonIpcFrame(PickCore::encodeDaemonIpcFrame(ping));
    REQUIRE(decodedPing.has_value());
    CHECK(decodedPing->type == PickCore::DaemonIpcMessageType::Ping);
    CHECK(decodedPing->payload.empty());

    PickCore::DaemonIpcFrame pong{};
    pong.type = PickCore::DaemonIpcMessageType::Pong;
    const std::optional<PickCore::DaemonIpcFrame> decodedPong =
        PickCore::decodeDaemonIpcFrame(PickCore::encodeDaemonIpcFrame(pong));
    REQUIRE(decodedPong.has_value());
    CHECK(decodedPong->type == PickCore::DaemonIpcMessageType::Pong);
}

TEST_CASE("DaemonIpcProtocol rejects invalid magic") {
    std::vector<std::uint8_t> bytes = PickCore::encodeDaemonIpcFrame(PickCore::DaemonIpcFrame{});
    bytes[0] = 'X';
    CHECK_FALSE(PickCore::decodeDaemonIpcFrame(bytes).has_value());
}

#ifndef _WIN32
TEST_CASE("GeminiDaemonRunner IPC client handshake ping pong") {
    PickCore::DaemonConfig config{};
    config.maxSessions = 4;
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

    const int clientFd = connectUnixSocket(config.socketPath);
    REQUIRE(clientFd >= 0);

    PickCore::DaemonIpcFrame handshake{};
    handshake.type = PickCore::DaemonIpcMessageType::Handshake;
    handshake.payload = PickCore::encodeHandshakePayload(PickCore::DaemonIpcHandshakePayload{1});
    sendFrame(clientFd, handshake);

    const PickCore::DaemonIpcFrame ack = recvFrame(clientFd);
    CHECK(ack.type == PickCore::DaemonIpcMessageType::HandshakeAck);
    const std::optional<PickCore::DaemonIpcHandshakeAckPayload> ackPayload =
        PickCore::decodeHandshakeAckPayload(ack.payload);
    REQUIRE(ackPayload.has_value());
    CHECK(ackPayload->serverProtocolVersion == 1);
    CHECK(ackPayload->maxSessions == 4);
    CHECK_FALSE(ackPayload->buildVersion.empty());

    PickCore::DaemonIpcFrame ping{};
    ping.type = PickCore::DaemonIpcMessageType::Ping;
    sendFrame(clientFd, ping);
    const PickCore::DaemonIpcFrame pong = recvFrame(clientFd);
    CHECK(pong.type == PickCore::DaemonIpcMessageType::Pong);

    ::close(clientFd);

    runner.requestShutdown();
    runnerThread.join();
    CHECK_FALSE(std::filesystem::exists(config.socketPath));
}

TEST_CASE("GeminiDaemonRunner IPC reserve session stub") {
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

    const int clientFd = connectUnixSocket(config.socketPath);
    REQUIRE(clientFd >= 0);

    PickCore::DaemonIpcFrame handshake{};
    handshake.type = PickCore::DaemonIpcMessageType::Handshake;
    handshake.payload = PickCore::encodeHandshakePayload(PickCore::DaemonIpcHandshakePayload{1});
    sendFrame(clientFd, handshake);
    (void) recvFrame(clientFd);

    PickCore::DaemonIpcFrame reserve{};
    reserve.type = PickCore::DaemonIpcMessageType::ReserveSession;
    sendFrame(clientFd, reserve);
    const PickCore::DaemonIpcFrame reserveAck = recvFrame(clientFd);
    CHECK(reserveAck.type == PickCore::DaemonIpcMessageType::ReserveSessionAck);
    const std::optional<PickCore::DaemonIpcReserveSessionAckPayload> reservePayload =
        PickCore::decodeReserveSessionAckPayload(reserveAck.payload);
    REQUIRE(reservePayload.has_value());
    CHECK(reservePayload->sessionPort == 1);
    CHECK(host.sessions().count() == 1);

    ::close(clientFd);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("GeminiDaemonRunner IPC shutdown request stops daemon") {
    PickCore::DaemonConfig config{};
    config.maxSessions = 1;
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

    const int clientFd = connectUnixSocket(config.socketPath);
    REQUIRE(clientFd >= 0);

    PickCore::DaemonIpcFrame handshake{};
    handshake.type = PickCore::DaemonIpcMessageType::Handshake;
    handshake.payload = PickCore::encodeHandshakePayload(PickCore::DaemonIpcHandshakePayload{1});
    sendFrame(clientFd, handshake);
    (void) recvFrame(clientFd);

    PickCore::DaemonIpcFrame shutdown{};
    shutdown.type = PickCore::DaemonIpcMessageType::ShutdownRequest;
    sendFrame(clientFd, shutdown);
    const PickCore::DaemonIpcFrame shutdownAck = recvFrame(clientFd);
    CHECK(shutdownAck.type == PickCore::DaemonIpcMessageType::ShutdownAck);
    ::close(clientFd);

    runnerThread.join();
    CHECK_FALSE(std::filesystem::exists(config.socketPath));
}

TEST_CASE("DaemonIpcServer socket permissions are owner-only") {
    PickCore::DaemonConfig config{};
    config.maxSessions = 1;
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

    struct stat st {};
    REQUIRE(::stat(config.socketPath.c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0600);

    runner.requestShutdown();
    runnerThread.join();
}
#endif
