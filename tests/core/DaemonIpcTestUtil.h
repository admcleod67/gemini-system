#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>

#include <doctest/doctest.h>

#include "DaemonIpcProtocol.h"

#ifndef _WIN32
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
#endif

namespace PickTests {
    inline std::filesystem::path uniqueDaemonIpcTempDir() {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() / ("pick-daemon-ipc-" + std::to_string(tick));
    }

#ifndef _WIN32
    inline int connectUnixSocket(const std::filesystem::path &socketPath) {
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            return -1;
        }

        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        const std::string path = socketPath.string();
        if (path.size() >= sizeof(address.sun_path)) {
            ::close(fd);
            return -1;
        }
        std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
        if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
            ::close(fd);
            return -1;
        }
        return fd;
    }

    inline bool waitForSocket(const std::filesystem::path &socketPath, const std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            std::error_code ec;
            if (std::filesystem::exists(socketPath, ec) && !ec) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    inline void sendFrame(const int fd, const PickCore::DaemonIpcFrame &frame) {
        REQUIRE(PickCore::writeDaemonIpcFrame(fd, frame));
    }

    inline PickCore::DaemonIpcFrame recvFrame(const int fd) {
        const std::optional<PickCore::DaemonIpcFrame> frame = PickCore::readDaemonIpcFrame(fd);
        REQUIRE(frame.has_value());
        return *frame;
    }

    inline void sendHandshake(const int fd, const std::uint16_t protocolVersion = PickCore::kDaemonIpcProtocolVersion) {
        PickCore::DaemonIpcFrame handshake{};
        handshake.type = PickCore::DaemonIpcMessageType::Handshake;
        handshake.payload = PickCore::encodeHandshakePayload(PickCore::DaemonIpcHandshakePayload{protocolVersion});
        sendFrame(fd, handshake);
    }

    inline PickCore::DaemonIpcFrame recvHandshakeAck(const int fd) {
        const PickCore::DaemonIpcFrame ack = recvFrame(fd);
        REQUIRE(ack.type == PickCore::DaemonIpcMessageType::HandshakeAck);
        return ack;
    }
#endif
} // namespace PickTests
