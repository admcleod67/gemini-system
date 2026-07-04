#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "DaemonIpcProtocol.h"

namespace {
    PickCore::DaemonIpcFrame makeFrame(const PickCore::DaemonIpcMessageType type,
                                       std::vector<std::uint8_t> payload = {}) {
        PickCore::DaemonIpcFrame frame{};
        frame.type = type;
        frame.payload = std::move(payload);
        return frame;
    }

    void checkRoundTripFrame(const PickCore::DaemonIpcFrame &frame) {
        const std::vector<std::uint8_t> bytes = PickCore::encodeDaemonIpcFrame(frame);
        const std::optional<PickCore::DaemonIpcFrame> decoded = PickCore::decodeDaemonIpcFrame(bytes);
        REQUIRE(decoded.has_value());
        CHECK(decoded->version == PickCore::kDaemonIpcProtocolVersion);
        CHECK(decoded->type == frame.type);
        CHECK(decoded->payload == frame.payload);
    }
} // namespace

TEST_CASE("DaemonIpcSession encodes and decodes AttachSession payload") {
    for (const PickCore::SessionId port : {0U, 42U}) {
        PickCore::DaemonIpcAttachSessionPayload attach{};
        attach.requestedPort = port;
        const std::vector<std::uint8_t> encoded = PickCore::encodeAttachSessionPayload(attach);
        const std::optional<PickCore::DaemonIpcAttachSessionPayload> decoded =
            PickCore::decodeAttachSessionPayload(encoded);
        REQUIRE(decoded.has_value());
        CHECK(decoded->requestedPort == port);
    }

    CHECK_FALSE(PickCore::decodeAttachSessionPayload({0, 0, 0}).has_value());
}

TEST_CASE("DaemonIpcSession encodes and decodes AttachSessionAck payload") {
    PickCore::DaemonIpcAttachSessionAckPayload ack{};
    ack.sessionPort = 7;
    const std::optional<PickCore::DaemonIpcAttachSessionAckPayload> decoded =
        PickCore::decodeAttachSessionAckPayload(PickCore::encodeAttachSessionAckPayload(ack));
    REQUIRE(decoded.has_value());
    CHECK(decoded->sessionPort == 7);
}

TEST_CASE("DaemonIpcSession encodes and decodes DetachSession and DetachSessionAck frames") {
    checkRoundTripFrame(makeFrame(PickCore::DaemonIpcMessageType::DetachSession));
    checkRoundTripFrame(makeFrame(PickCore::DaemonIpcMessageType::DetachSessionAck));
}

TEST_CASE("DaemonIpcSession encodes and decodes SessionInput payload") {
    PickCore::DaemonIpcSessionDataPayload empty{};
    REQUIRE(PickCore::decodeSessionDataPayload(PickCore::encodeSessionDataPayload(empty)).has_value());

    PickCore::DaemonIpcSessionDataPayload single{{'A'}};
    const std::optional<PickCore::DaemonIpcSessionDataPayload> singleDecoded =
        PickCore::decodeSessionDataPayload(PickCore::encodeSessionDataPayload(single));
    REQUIRE(singleDecoded.has_value());
    CHECK(singleDecoded->data == single.data);

    PickCore::DaemonIpcSessionDataPayload multi{};
    multi.data = {'L', 'O', 'G', 'O', 'N', ' ', 'P', 'L', 'E', 'A', 'S', 'E', ':'};
    const std::optional<PickCore::DaemonIpcSessionDataPayload> multiDecoded =
        PickCore::decodeSessionDataPayload(PickCore::encodeSessionDataPayload(multi));
    REQUIRE(multiDecoded.has_value());
    CHECK(multiDecoded->data == multi.data);

    PickCore::DaemonIpcSessionDataPayload maxChunk{};
    maxChunk.data.assign(PickCore::kDaemonIpcMaxSessionDataSize, 0xAB);
    const std::optional<PickCore::DaemonIpcSessionDataPayload> maxDecoded =
        PickCore::decodeSessionDataPayload(PickCore::encodeSessionDataPayload(maxChunk));
    REQUIRE(maxDecoded.has_value());
    CHECK(maxDecoded->data.size() == PickCore::kDaemonIpcMaxSessionDataSize);
}

TEST_CASE("DaemonIpcSession encodes and decodes SessionOutput and SessionDiagnostic payloads") {
    PickCore::DaemonIpcSessionDataPayload output{};
    output.data = {'T', 'C', 'L', '>'};
    const std::optional<PickCore::DaemonIpcSessionDataPayload> outputDecoded =
        PickCore::decodeSessionDataPayload(PickCore::encodeSessionDataPayload(output));
    REQUIRE(outputDecoded.has_value());
    CHECK(outputDecoded->data == output.data);

    PickCore::DaemonIpcSessionDataPayload diagnostic{};
    diagnostic.data = {'w', 'a', 'r', 'n', '\n'};
    const std::optional<PickCore::DaemonIpcSessionDataPayload> diagnosticDecoded =
        PickCore::decodeSessionDataPayload(PickCore::encodeSessionDataPayload(diagnostic));
    REQUIRE(diagnosticDecoded.has_value());
    CHECK(diagnosticDecoded->data == diagnostic.data);
}

TEST_CASE("DaemonIpcSession rejects invalid session data payloads") {
    CHECK_FALSE(PickCore::decodeSessionDataPayload({}).has_value());
    CHECK_FALSE(PickCore::decodeSessionDataPayload({0, 0, 0, 5, 1, 2}).has_value());

    PickCore::DaemonIpcSessionDataPayload oversize{};
    oversize.data.assign(PickCore::kDaemonIpcMaxSessionDataSize + 1, 0x01);
    CHECK_FALSE(PickCore::decodeSessionDataPayload(PickCore::encodeSessionDataPayload(oversize)).has_value());

    std::vector<std::uint8_t> declaredTooLarge(4);
    declaredTooLarge[3] = static_cast<std::uint8_t>(PickCore::kDaemonIpcMaxSessionDataSize + 1);
    CHECK_FALSE(PickCore::decodeSessionDataPayload(declaredTooLarge).has_value());
}

TEST_CASE("DaemonIpcSession full frame round-trip for session plane message types") {
    PickCore::DaemonIpcAttachSessionPayload attach{};
    attach.requestedPort = 0;
    checkRoundTripFrame(makeFrame(PickCore::DaemonIpcMessageType::AttachSession,
                                  PickCore::encodeAttachSessionPayload(attach)));

    PickCore::DaemonIpcAttachSessionAckPayload attachAck{};
    attachAck.sessionPort = 3;
    checkRoundTripFrame(makeFrame(PickCore::DaemonIpcMessageType::AttachSessionAck,
                                  PickCore::encodeAttachSessionAckPayload(attachAck)));

    PickCore::DaemonIpcSessionDataPayload input{};
    input.data = {'Q', 'U', 'I', 'T', '\n'};
    checkRoundTripFrame(
        makeFrame(PickCore::DaemonIpcMessageType::SessionInput, PickCore::encodeSessionDataPayload(input)));

    PickCore::DaemonIpcSessionDataPayload sessionOut{};
    sessionOut.data = {'1', ' ', 'S', 'Y', 'S', 'P', 'R', 'O', 'G'};
    checkRoundTripFrame(
        makeFrame(PickCore::DaemonIpcMessageType::SessionOutput, PickCore::encodeSessionDataPayload(sessionOut)));

    checkRoundTripFrame(makeFrame(PickCore::DaemonIpcMessageType::SessionDiagnostic,
                                  PickCore::encodeSessionDataPayload(sessionOut)));
}

TEST_CASE("DaemonIpcSession encodes and decodes M14 error codes") {
    for (const PickCore::DaemonIpcErrorCode code :
         {PickCore::DaemonIpcErrorCode::SessionNotFound, PickCore::DaemonIpcErrorCode::SessionAlreadyBound,
          PickCore::DaemonIpcErrorCode::NotAttached}) {
        PickCore::DaemonIpcErrorPayload error{};
        error.code = code;
        error.message = "session plane error";
        const std::optional<PickCore::DaemonIpcErrorPayload> decoded =
            PickCore::decodeErrorPayload(PickCore::encodeErrorPayload(error));
        REQUIRE(decoded.has_value());
        CHECK(decoded->code == code);
        CHECK(decoded->message == error.message);
    }
}
