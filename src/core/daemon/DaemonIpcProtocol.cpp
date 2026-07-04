#include "DaemonIpcProtocol.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <unistd.h>

namespace PickCore {
    namespace {
        [[nodiscard]] std::uint16_t readU16(const std::uint8_t *data) {
            return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) | data[1]);
        }

        [[nodiscard]] std::uint32_t readU32(const std::uint8_t *data) {
            return (static_cast<std::uint32_t>(data[0]) << 24) | (static_cast<std::uint32_t>(data[1]) << 16) |
                   (static_cast<std::uint32_t>(data[2]) << 8) | static_cast<std::uint32_t>(data[3]);
        }

        void writeU16(const std::uint16_t value, std::vector<std::uint8_t> &out) {
            out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
            out.push_back(static_cast<std::uint8_t>(value & 0xFF));
        }

        void writeU32(const std::uint32_t value, std::vector<std::uint8_t> &out) {
            out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
            out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
            out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
            out.push_back(static_cast<std::uint8_t>(value & 0xFF));
        }

        [[nodiscard]] bool magicMatches(const std::array<char, 4> &magic) {
            return magic[0] == kDaemonIpcMagic[0] && magic[1] == kDaemonIpcMagic[1] &&
                   magic[2] == kDaemonIpcMagic[2] && magic[3] == kDaemonIpcMagic[3];
        }
    } // namespace

    bool readExact(const int fd, void *const buffer, const std::size_t length) {
        if (length == 0) {
            return true;
        }
        auto *cursor = static_cast<std::uint8_t *>(buffer);
        std::size_t remaining = length;
        while (remaining > 0) {
            const auto chunk = ::read(fd, cursor, remaining);
            if (chunk <= 0) {
                return false;
            }
            cursor += chunk;
            remaining -= static_cast<std::size_t>(chunk);
        }
        return true;
    }

    bool writeExact(const int fd, const void *const buffer, const std::size_t length) {
        if (length == 0) {
            return true;
        }
        const auto *cursor = static_cast<const std::uint8_t *>(buffer);
        std::size_t remaining = length;
        while (remaining > 0) {
            const auto chunk = ::write(fd, cursor, remaining);
            if (chunk <= 0) {
                return false;
            }
            cursor += chunk;
            remaining -= static_cast<std::size_t>(chunk);
        }
        return true;
    }

    std::vector<std::uint8_t> encodeDaemonIpcFrame(const DaemonIpcFrame &frame) {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(12 + frame.payload.size());
        bytes.insert(bytes.end(), kDaemonIpcMagic, kDaemonIpcMagic + 4);
        writeU16(frame.version, bytes);
        writeU16(static_cast<std::uint16_t>(frame.type), bytes);
        writeU32(static_cast<std::uint32_t>(frame.payload.size()), bytes);
        bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());
        return bytes;
    }

    std::optional<DaemonIpcFrame> decodeDaemonIpcFrame(const std::vector<std::uint8_t> &bytes) {
        if (bytes.size() < 12) {
            return std::nullopt;
        }
        if (bytes[0] != kDaemonIpcMagic[0] || bytes[1] != kDaemonIpcMagic[1] || bytes[2] != kDaemonIpcMagic[2] ||
            bytes[3] != kDaemonIpcMagic[3]) {
            return std::nullopt;
        }

        DaemonIpcFrame frame{};
        frame.version = readU16(bytes.data() + 4);
        frame.type = static_cast<DaemonIpcMessageType>(readU16(bytes.data() + 6));
        const std::uint32_t payloadLen = readU32(bytes.data() + 8);
        if (payloadLen > kDaemonIpcMaxPayloadSize || bytes.size() < 12 + payloadLen) {
            return std::nullopt;
        }
        frame.payload.assign(bytes.begin() + 12, bytes.begin() + 12 + payloadLen);
        return frame;
    }

    std::optional<DaemonIpcFrame> readDaemonIpcFrame(const int fd) {
        std::array<char, 4> magic{};
        if (!readExact(fd, magic.data(), magic.size())) {
            return std::nullopt;
        }
        if (!magicMatches(magic)) {
            return std::nullopt;
        }

        std::array<std::uint8_t, 8> header{};
        if (!readExact(fd, header.data(), header.size())) {
            return std::nullopt;
        }

        DaemonIpcFrame frame{};
        frame.version = readU16(header.data());
        frame.type = static_cast<DaemonIpcMessageType>(readU16(header.data() + 2));
        const std::uint32_t payloadLen = readU32(header.data() + 4);
        if (payloadLen > kDaemonIpcMaxPayloadSize) {
            return std::nullopt;
        }

        frame.payload.resize(payloadLen);
        if (payloadLen > 0 && !readExact(fd, frame.payload.data(), payloadLen)) {
            return std::nullopt;
        }
        return frame;
    }

    bool writeDaemonIpcFrame(const int fd, const DaemonIpcFrame &frame) {
        const std::vector<std::uint8_t> bytes = encodeDaemonIpcFrame(frame);
        return writeExact(fd, bytes.data(), bytes.size());
    }

    std::vector<std::uint8_t> encodeHandshakePayload(const DaemonIpcHandshakePayload &payload) {
        std::vector<std::uint8_t> bytes;
        writeU16(payload.clientProtocolVersion, bytes);
        return bytes;
    }

    std::optional<DaemonIpcHandshakePayload> decodeHandshakePayload(const std::vector<std::uint8_t> &payload) {
        if (payload.size() != 2) {
            return std::nullopt;
        }
        DaemonIpcHandshakePayload out{};
        out.clientProtocolVersion = readU16(payload.data());
        return out;
    }

    std::vector<std::uint8_t> encodeHandshakeAckPayload(const DaemonIpcHandshakeAckPayload &payload) {
        std::vector<std::uint8_t> bytes;
        writeU16(payload.serverProtocolVersion, bytes);
        writeU16(payload.maxSessions, bytes);
        writeU32(static_cast<std::uint32_t>(payload.buildVersion.size()), bytes);
        bytes.insert(bytes.end(), payload.buildVersion.begin(), payload.buildVersion.end());
        return bytes;
    }

    std::optional<DaemonIpcHandshakeAckPayload> decodeHandshakeAckPayload(const std::vector<std::uint8_t> &payload) {
        if (payload.size() < 8) {
            return std::nullopt;
        }
        DaemonIpcHandshakeAckPayload out{};
        out.serverProtocolVersion = readU16(payload.data());
        out.maxSessions = readU16(payload.data() + 2);
        const std::uint32_t versionLen = readU32(payload.data() + 4);
        if (versionLen > kDaemonIpcMaxPayloadSize || payload.size() != 8 + versionLen) {
            return std::nullopt;
        }
        out.buildVersion.assign(payload.begin() + 8, payload.begin() + 8 + versionLen);
        return out;
    }

    std::vector<std::uint8_t> encodeReserveSessionAckPayload(const DaemonIpcReserveSessionAckPayload &payload) {
        std::vector<std::uint8_t> bytes;
        writeU32(payload.sessionPort, bytes);
        return bytes;
    }

    std::optional<DaemonIpcReserveSessionAckPayload>
    decodeReserveSessionAckPayload(const std::vector<std::uint8_t> &payload) {
        if (payload.size() != 4) {
            return std::nullopt;
        }
        DaemonIpcReserveSessionAckPayload out{};
        out.sessionPort = readU32(payload.data());
        return out;
    }

    std::vector<std::uint8_t> encodeErrorPayload(const DaemonIpcErrorPayload &payload) {
        std::vector<std::uint8_t> bytes;
        writeU32(static_cast<std::uint32_t>(payload.code), bytes);
        writeU32(static_cast<std::uint32_t>(payload.message.size()), bytes);
        bytes.insert(bytes.end(), payload.message.begin(), payload.message.end());
        return bytes;
    }

    std::optional<DaemonIpcErrorPayload> decodeErrorPayload(const std::vector<std::uint8_t> &payload) {
        if (payload.size() < 8) {
            return std::nullopt;
        }
        DaemonIpcErrorPayload out{};
        out.code = static_cast<DaemonIpcErrorCode>(readU32(payload.data()));
        const std::uint32_t messageLen = readU32(payload.data() + 4);
        if (messageLen > kDaemonIpcMaxPayloadSize || payload.size() != 8 + messageLen) {
            return std::nullopt;
        }
        out.message.assign(payload.begin() + 8, payload.begin() + 8 + messageLen);
        return out;
    }

    std::vector<std::uint8_t> encodeAttachSessionPayload(const DaemonIpcAttachSessionPayload &payload) {
        std::vector<std::uint8_t> bytes;
        writeU32(payload.requestedPort, bytes);
        return bytes;
    }

    std::optional<DaemonIpcAttachSessionPayload> decodeAttachSessionPayload(const std::vector<std::uint8_t> &payload) {
        if (payload.size() != 4) {
            return std::nullopt;
        }
        DaemonIpcAttachSessionPayload out{};
        out.requestedPort = readU32(payload.data());
        return out;
    }

    std::vector<std::uint8_t> encodeAttachSessionAckPayload(const DaemonIpcAttachSessionAckPayload &payload) {
        std::vector<std::uint8_t> bytes;
        writeU32(payload.sessionPort, bytes);
        return bytes;
    }

    std::optional<DaemonIpcAttachSessionAckPayload>
    decodeAttachSessionAckPayload(const std::vector<std::uint8_t> &payload) {
        if (payload.size() != 4) {
            return std::nullopt;
        }
        DaemonIpcAttachSessionAckPayload out{};
        out.sessionPort = readU32(payload.data());
        return out;
    }

    std::vector<std::uint8_t> encodeSessionDataPayload(const DaemonIpcSessionDataPayload &payload) {
        std::vector<std::uint8_t> bytes;
        writeU32(static_cast<std::uint32_t>(payload.data.size()), bytes);
        bytes.insert(bytes.end(), payload.data.begin(), payload.data.end());
        return bytes;
    }

    std::optional<DaemonIpcSessionDataPayload> decodeSessionDataPayload(const std::vector<std::uint8_t> &payload) {
        if (payload.size() < 4) {
            return std::nullopt;
        }
        const std::uint32_t dataLen = readU32(payload.data());
        if (dataLen > kDaemonIpcMaxSessionDataSize || payload.size() != 4 + dataLen) {
            return std::nullopt;
        }
        DaemonIpcSessionDataPayload out{};
        out.data.assign(payload.begin() + 4, payload.begin() + 4 + dataLen);
        return out;
    }
} // namespace PickCore
