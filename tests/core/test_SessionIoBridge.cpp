#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "IpcSessionChannel.h"

#ifndef _WIN32
    #include <fcntl.h>
    #include <poll.h>
    #include <unistd.h>

    #include <filesystem>
    #include <sstream>

    #include "DaemonConfig.h"
    #include "DaemonIpcProtocol.h"
    #include "DaemonIpcTestUtil.h"
    #include "GeminiDaemonRunner.h"
    #include "GeminiServiceDaemon.h"
    #include "GeminiSessionHost.h"

using PickTests::connectUnixSocket;
using PickTests::recvFrame;
using PickTests::recvHandshakeAck;
using PickTests::sendFrame;
using PickTests::sendHandshake;
using PickTests::uniqueDaemonIpcTempDir;
using PickTests::waitForSocket;

namespace {
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

    std::vector<std::uint8_t> recvSessionOutput(const int clientFd) {
        const PickCore::DaemonIpcFrame frame = recvFrame(clientFd);
        REQUIRE(frame.type == PickCore::DaemonIpcMessageType::SessionOutput);
        const std::optional<PickCore::DaemonIpcSessionDataPayload> payload =
            PickCore::decodeSessionDataPayload(frame.payload);
        REQUIRE(payload.has_value());
        return payload->data;
    }

    std::string drainSessionOutput(const int clientFd) {
        std::string result;
        int idlePolls = 0;
        while (idlePolls < 10) {
            pollfd pfd{};
            pfd.fd = clientFd;
            pfd.events = POLLIN;
            const int pollResult = ::poll(&pfd, 1, 50);
            if (pollResult <= 0 || (pfd.revents & POLLIN) == 0) {
                ++idlePolls;
                continue;
            }
            idlePolls = 0;
            const std::vector<std::uint8_t> chunk = recvSessionOutput(clientFd);
            result.append(reinterpret_cast<const char *>(chunk.data()), chunk.size());
        }
        return result;
    }
} // namespace

TEST_CASE("IpcSessionChannel reads pushed input bytes") {
    PickCore::IpcSessionChannel channel;
    const std::vector<std::uint8_t> bytes = {'h', 'i'};
    channel.pushInput(bytes);

    CHECK(channel.input().get() == 'h');
    CHECK(channel.input().get() == 'i');
}

TEST_CASE("IpcSessionChannel flushPendingOutput sends SessionOutput frames") {
#ifndef _WIN32
    int fds[2] = {-1, -1};
    REQUIRE(::pipe(fds) == 0);

    const int readFd = fds[0];
    const int writeFd = fds[1];
    const int flags = ::fcntl(readFd, F_GETFL, 0);
    REQUIRE(flags >= 0);
    REQUIRE(::fcntl(readFd, F_SETFL, flags | O_NONBLOCK) == 0);

    PickCore::IpcSessionChannel channel;
    channel.output() << "ab";
    channel.output().flush();
    channel.flushPendingOutput(writeFd);

    const std::optional<PickCore::DaemonIpcFrame> frame = PickCore::readDaemonIpcFrame(readFd);
    REQUIRE(frame.has_value());
    CHECK(frame->type == PickCore::DaemonIpcMessageType::SessionOutput);
    const std::optional<PickCore::DaemonIpcSessionDataPayload> payload =
        PickCore::decodeSessionDataPayload(frame->payload);
    REQUIRE(payload.has_value());
    CHECK(payload->data == std::vector<std::uint8_t>({'a', 'b'}));

    ::close(readFd);
    ::close(writeFd);
#endif
}

TEST_CASE("IpcSessionChannel close unblocks blocked input reader with EOF") {
    PickCore::IpcSessionChannel channel;
    std::atomic<bool> finished{false};
    std::thread reader([&] {
        CHECK(channel.input().peek() == std::char_traits<char>::eof());
        finished.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    channel.close();
    reader.join();
    CHECK(finished.load());
}

TEST_CASE("GeminiDaemonRunner attach creates session and returns port") {
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

    sendHandshake(clientFd);
    recvHandshakeAck(clientFd);

    const PickCore::SessionId port = attachNewSession(clientFd);
    CHECK(port > 0);
    CHECK(host.sessions().find(port) != nullptr);

    ::close(clientFd);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("GeminiDaemonRunner rejects SessionInput before attach") {
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

    sendHandshake(clientFd);
    recvHandshakeAck(clientFd);

    sendSessionInput(clientFd, {'x'});

    const PickCore::DaemonIpcFrame errorFrame = recvFrame(clientFd);
    CHECK(errorFrame.type == PickCore::DaemonIpcMessageType::Error);
    const std::optional<PickCore::DaemonIpcErrorPayload> errorPayload =
        PickCore::decodeErrorPayload(errorFrame.payload);
    REQUIRE(errorPayload.has_value());
    CHECK(errorPayload->code == PickCore::DaemonIpcErrorCode::NotAttached);

    ::close(clientFd);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("GeminiDaemonRunner attach unknown port returns SessionNotFound") {
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

    sendHandshake(clientFd);
    recvHandshakeAck(clientFd);

    PickCore::DaemonIpcAttachSessionPayload attachPayload{};
    attachPayload.requestedPort = 999;
    PickCore::DaemonIpcFrame attach{};
    attach.type = PickCore::DaemonIpcMessageType::AttachSession;
    attach.payload = PickCore::encodeAttachSessionPayload(attachPayload);
    sendFrame(clientFd, attach);

    const PickCore::DaemonIpcFrame errorFrame = recvFrame(clientFd);
    CHECK(errorFrame.type == PickCore::DaemonIpcMessageType::Error);
    const std::optional<PickCore::DaemonIpcErrorPayload> errorPayload =
        PickCore::decodeErrorPayload(errorFrame.payload);
    REQUIRE(errorPayload.has_value());
    CHECK(errorPayload->code == PickCore::DaemonIpcErrorCode::SessionNotFound);

    ::close(clientFd);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("GeminiDaemonRunner rejects second attach to same session port") {
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

    const int clientA = connectUnixSocket(config.socketPath);
    const int clientB = connectUnixSocket(config.socketPath);
    REQUIRE(clientA >= 0);
    REQUIRE(clientB >= 0);

    sendHandshake(clientA);
    sendHandshake(clientB);
    recvHandshakeAck(clientA);
    recvHandshakeAck(clientB);

    const PickCore::SessionId port = attachNewSession(clientA);

    PickCore::DaemonIpcAttachSessionPayload attachPayload{};
    attachPayload.requestedPort = port;
    PickCore::DaemonIpcFrame attach{};
    attach.type = PickCore::DaemonIpcMessageType::AttachSession;
    attach.payload = PickCore::encodeAttachSessionPayload(attachPayload);
    sendFrame(clientB, attach);

    const PickCore::DaemonIpcFrame errorFrame = recvFrame(clientB);
    CHECK(errorFrame.type == PickCore::DaemonIpcMessageType::Error);
    const std::optional<PickCore::DaemonIpcErrorPayload> errorPayload =
        PickCore::decodeErrorPayload(errorFrame.payload);
    REQUIRE(errorPayload.has_value());
    CHECK(errorPayload->code == PickCore::DaemonIpcErrorCode::SessionAlreadyBound);

    ::close(clientA);
    ::close(clientB);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("GeminiDaemonRunner detach clears attach and rejects further SessionInput") {
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

    sendHandshake(clientFd);
    recvHandshakeAck(clientFd);
    attachNewSession(clientFd);

    PickCore::DaemonIpcFrame detach{};
    detach.type = PickCore::DaemonIpcMessageType::DetachSession;
    sendFrame(clientFd, detach);
    CHECK(recvFrame(clientFd).type == PickCore::DaemonIpcMessageType::DetachSessionAck);

    sendSessionInput(clientFd, {'x'});
    const PickCore::DaemonIpcFrame errorFrame = recvFrame(clientFd);
    CHECK(errorFrame.type == PickCore::DaemonIpcMessageType::Error);
    const std::optional<PickCore::DaemonIpcErrorPayload> errorPayload =
        PickCore::decodeErrorPayload(errorFrame.payload);
    REQUIRE(errorPayload.has_value());
    CHECK(errorPayload->code == PickCore::DaemonIpcErrorCode::NotAttached);

    ::close(clientFd);
    runner.requestShutdown();
    runnerThread.join();
}

TEST_CASE("GeminiDaemonRunner session I/O bridge round-trips bytes over IPC") {
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

    sendHandshake(clientFd);
    recvHandshakeAck(clientFd);

    (void) attachNewSession(clientFd);

    const std::string banner = drainSessionOutput(clientFd);
    CHECK(banner.find("Gemini/TCL Developer Shell") != std::string::npos);

    sendSessionInput(clientFd, {'W', 'H', 'O', '\n', 'Q', 'U', 'I', 'T', '\n'});
    const std::string commandOutput = drainSessionOutput(clientFd);
    CHECK(commandOutput.find("0 - -\n") != std::string::npos);

    ::close(clientFd);
    runner.requestShutdown();
    runnerThread.join();
}

#endif
