//
// Gemini daemon IPC wire protocol v1 (Milestone 13 Stage 5).
//
// Transport: Unix domain stream socket (AF_UNIX).
// Byte order: network (big-endian) for all multi-byte fields.
//
// Frame layout:
//   magic[4]   = "GEMI"
//   version    = uint16  (protocol version, currently 1)
//   type       = uint16  (DaemonIpcMessageType)
//   payloadLen = uint32
//   payload[]  = type-specific (may be empty)
//
// Client must send Handshake first. Further messages on the same connection
// require a completed handshake. Login/REPL byte streams are Milestone 14.
//

#ifndef PICK_SYSTEM_CORE_DAEMON_DAEMON_IPC_PROTOCOL_H
#define PICK_SYSTEM_CORE_DAEMON_DAEMON_IPC_PROTOCOL_H

#include "SerialSessionRunner.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace PickCore {
    inline constexpr std::uint16_t kDaemonIpcProtocolVersion = 1;
    inline constexpr std::size_t kDaemonIpcMaxPayloadSize = 65536;
    inline constexpr char kDaemonIpcMagic[4] = {'G', 'E', 'M', 'I'};

    enum class DaemonIpcMessageType : std::uint16_t {
        Handshake = 1,
        HandshakeAck = 2,
        Ping = 3,
        Pong = 4,
        ShutdownRequest = 5,
        ShutdownAck = 6,
        ReserveSession = 7,
        ReserveSessionAck = 8,
        Error = 9,
    };

    enum class DaemonIpcErrorCode : std::uint32_t {
        UnsupportedVersion = 1,
        InvalidMessage = 2,
        SessionTableFull = 3,
        NotHandshaken = 4,
    };

    struct DaemonIpcFrame {
        std::uint16_t version{kDaemonIpcProtocolVersion};
        DaemonIpcMessageType type{DaemonIpcMessageType::Ping};
        std::vector<std::uint8_t> payload;
    };

    struct DaemonIpcHandshakePayload {
        std::uint16_t clientProtocolVersion{0};
    };

    struct DaemonIpcHandshakeAckPayload {
        std::uint16_t serverProtocolVersion{kDaemonIpcProtocolVersion};
        std::uint16_t maxSessions{0};
        std::string buildVersion;
    };

    struct DaemonIpcReserveSessionAckPayload {
        SessionId sessionPort{0};
    };

    struct DaemonIpcErrorPayload {
        DaemonIpcErrorCode code{DaemonIpcErrorCode::InvalidMessage};
        std::string message;
    };

    [[nodiscard]] std::vector<std::uint8_t> encodeDaemonIpcFrame(const DaemonIpcFrame &frame);
    [[nodiscard]] std::optional<DaemonIpcFrame> decodeDaemonIpcFrame(const std::vector<std::uint8_t> &bytes);

    [[nodiscard]] std::vector<std::uint8_t> encodeHandshakePayload(const DaemonIpcHandshakePayload &payload);
    [[nodiscard]] std::optional<DaemonIpcHandshakePayload> decodeHandshakePayload(const std::vector<std::uint8_t> &payload);

    [[nodiscard]] std::vector<std::uint8_t> encodeHandshakeAckPayload(const DaemonIpcHandshakeAckPayload &payload);
    [[nodiscard]] std::optional<DaemonIpcHandshakeAckPayload> decodeHandshakeAckPayload(const std::vector<std::uint8_t> &payload);

    [[nodiscard]] std::vector<std::uint8_t> encodeReserveSessionAckPayload(const DaemonIpcReserveSessionAckPayload &payload);
    [[nodiscard]] std::optional<DaemonIpcReserveSessionAckPayload>
    decodeReserveSessionAckPayload(const std::vector<std::uint8_t> &payload);

    [[nodiscard]] std::vector<std::uint8_t> encodeErrorPayload(const DaemonIpcErrorPayload &payload);
    [[nodiscard]] std::optional<DaemonIpcErrorPayload> decodeErrorPayload(const std::vector<std::uint8_t> &payload);

    [[nodiscard]] bool readExact(int fd, void *buffer, std::size_t length);
    [[nodiscard]] bool writeExact(int fd, const void *buffer, std::size_t length);
    [[nodiscard]] std::optional<DaemonIpcFrame> readDaemonIpcFrame(int fd);
    [[nodiscard]] bool writeDaemonIpcFrame(int fd, const DaemonIpcFrame &frame);
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_DAEMON_IPC_PROTOCOL_H
