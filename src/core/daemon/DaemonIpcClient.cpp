#include "DaemonIpcClient.h"

#ifndef _WIN32

    #include <algorithm>
    #include <array>
    #include <cerrno>
    #include <cstring>
    #include <fcntl.h>
    #include <iostream>
    #include <poll.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>

namespace PickCore {
    namespace {
        void setNonBlocking(const int fd) {
            const int flags = ::fcntl(fd, F_GETFL, 0);
            if (flags >= 0) {
                (void) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }
        }

        void suppressSigPipeOnSocket(const int fd) {
#ifdef SO_NOSIGPIPE
            const int enabled = 1;
            (void) ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
        }

        [[nodiscard]] int inputFileDescriptor(std::istream &in) {
            if (&in == &std::cin) {
                return STDIN_FILENO;
            }
            return -1;
        }
    } // namespace

    DaemonIpcClient::DaemonIpcClient(std::filesystem::path socketPath) : socketPath_(std::move(socketPath)) {}

    DaemonIpcClient::~DaemonIpcClient() {
        disconnect();
    }

    void DaemonIpcClient::closeFd() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    void DaemonIpcClient::connect() {
        if (fd_ >= 0) {
            return;
        }

        fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0) {
            throw DaemonIpcClientError("failed to create socket");
        }

        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        const std::string path = socketPath_.string();
        if (path.size() >= sizeof(address.sun_path)) {
            closeFd();
            throw DaemonIpcClientError("socket path too long");
        }
        std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);

        if (::connect(fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
            closeFd();
            throw DaemonIpcClientError("failed to connect to daemon socket");
        }

        setNonBlocking(fd_);
        suppressSigPipeOnSocket(fd_);
    }

    void DaemonIpcClient::sendFrame(const DaemonIpcFrame &frame) {
        if (pendingWrite_.empty()) {
            pendingWrite_ = encodeDaemonIpcFrame(frame);
        } else {
            const std::vector<std::uint8_t> encoded = encodeDaemonIpcFrame(frame);
            pendingWrite_.insert(pendingWrite_.end(), encoded.begin(), encoded.end());
        }
        if (!flushPendingWrite()) {
            throw DaemonIpcClientError("failed to send IPC frame");
        }
    }

    bool DaemonIpcClient::flushPendingWrite() {
        while (!pendingWrite_.empty()) {
            const auto written = ::write(fd_, pendingWrite_.data(), pendingWrite_.size());
            if (written < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return true;
                }
                return false;
            }
            pendingWrite_.erase(pendingWrite_.begin(),
                                pendingWrite_.begin() + static_cast<std::ptrdiff_t>(written));
        }
        return true;
    }

    DaemonIpcFrame DaemonIpcClient::recvFrameBlocking() {
        for (;;) {
            const DaemonIpcTryReadResult result = tryReadDaemonIpcFrame(fd_, readBuffer_);
            if (result.status == DaemonIpcTryReadStatus::FrameReady && result.frame.has_value()) {
                return *result.frame;
            }
            if (result.status == DaemonIpcTryReadStatus::ConnectionClosed ||
                result.status == DaemonIpcTryReadStatus::ProtocolError) {
                throw DaemonIpcClientError("connection closed while reading IPC frame");
            }

            pollfd socketPoll{};
            socketPoll.fd = fd_;
            socketPoll.events = POLLIN;
            const int pollResult = ::poll(&socketPoll, 1, 100);
            if (pollResult < 0 && errno != EINTR) {
                throw DaemonIpcClientError("poll failed while waiting for IPC frame");
            }
            if ((socketPoll.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                throw DaemonIpcClientError("connection closed while waiting for IPC frame");
            }
        }
    }

    void DaemonIpcClient::handshake() {
        if (fd_ < 0) {
            throw DaemonIpcClientError("not connected");
        }

        DaemonIpcFrame handshake{};
        handshake.type = DaemonIpcMessageType::Handshake;
        handshake.payload =
            encodeHandshakePayload(DaemonIpcHandshakePayload{kDaemonIpcProtocolVersion});
        sendFrame(handshake);

        const DaemonIpcFrame ack = recvFrameBlocking();
        if (ack.type == DaemonIpcMessageType::Error) {
            const std::optional<DaemonIpcErrorPayload> error = decodeErrorPayload(ack.payload);
            const std::string message = error.has_value() ? error->message : "handshake failed";
            throw DaemonIpcClientError(message);
        }
        if (ack.type != DaemonIpcMessageType::HandshakeAck) {
            throw DaemonIpcClientError("unexpected response to handshake");
        }
    }

    SessionId DaemonIpcClient::attachSession(const SessionId requestedPort) {
        if (fd_ < 0) {
            throw DaemonIpcClientError("not connected");
        }
        if (attachedPort_.has_value()) {
            throw DaemonIpcClientError("already attached to a session");
        }

        DaemonIpcAttachSessionPayload payload{};
        payload.requestedPort = requestedPort;
        DaemonIpcFrame attach{};
        attach.type = DaemonIpcMessageType::AttachSession;
        attach.payload = encodeAttachSessionPayload(payload);
        sendFrame(attach);

        const DaemonIpcFrame response = recvFrameBlocking();
        if (response.type == DaemonIpcMessageType::Error) {
            const std::optional<DaemonIpcErrorPayload> error = decodeErrorPayload(response.payload);
            const std::string message = error.has_value() ? error->message : "attach session failed";
            throw DaemonIpcClientError(message);
        }
        if (response.type != DaemonIpcMessageType::AttachSessionAck) {
            throw DaemonIpcClientError("unexpected response to attach session");
        }

        const std::optional<DaemonIpcAttachSessionAckPayload> ack =
            decodeAttachSessionAckPayload(response.payload);
        if (!ack.has_value()) {
            throw DaemonIpcClientError("invalid attach session ack payload");
        }

        attachedPort_ = ack->sessionPort;
        return *attachedPort_;
    }

    void DaemonIpcClient::detachSession() {
        if (fd_ < 0 || !attachedPort_.has_value()) {
            attachedPort_.reset();
            return;
        }

        DaemonIpcFrame detach{};
        detach.type = DaemonIpcMessageType::DetachSession;
        sendFrame(detach);

        const DaemonIpcFrame response = recvFrameBlocking();
        if (response.type == DaemonIpcMessageType::Error) {
            const std::optional<DaemonIpcErrorPayload> error = decodeErrorPayload(response.payload);
            const std::string message = error.has_value() ? error->message : "detach session failed";
            throw DaemonIpcClientError(message);
        }
        if (response.type != DaemonIpcMessageType::DetachSessionAck) {
            throw DaemonIpcClientError("unexpected response to detach session");
        }

        attachedPort_.reset();
    }

    void DaemonIpcClient::disconnect() {
        if (fd_ < 0) {
            return;
        }

        try {
            if (attachedPort_.has_value()) {
                detachSession();
            }
        } catch (const DaemonIpcClientError &) {
            attachedPort_.reset();
        }

        closeFd();
        readBuffer_.clear();
        pendingWrite_.clear();
    }

    void DaemonIpcClient::dispatchIncomingFrame(const DaemonIpcFrame &frame,
                                                  std::ostream &out,
                                                  std::ostream &err) {
        switch (frame.type) {
            case DaemonIpcMessageType::SessionOutput: {
                const std::optional<DaemonIpcSessionDataPayload> payload =
                    decodeSessionDataPayload(frame.payload);
                if (payload.has_value() && !payload->data.empty()) {
                    out.write(reinterpret_cast<const char *>(payload->data.data()),
                              static_cast<std::streamsize>(payload->data.size()));
                    out.flush();
                }
                return;
            }
            case DaemonIpcMessageType::SessionDiagnostic: {
                const std::optional<DaemonIpcSessionDataPayload> payload =
                    decodeSessionDataPayload(frame.payload);
                if (payload.has_value() && !payload->data.empty()) {
                    err.write(reinterpret_cast<const char *>(payload->data.data()),
                              static_cast<std::streamsize>(payload->data.size()));
                    err.flush();
                }
                return;
            }
            case DaemonIpcMessageType::Error: {
                const std::optional<DaemonIpcErrorPayload> payload = decodeErrorPayload(frame.payload);
                const std::string message = payload.has_value() ? payload->message : "IPC error";
                throw DaemonIpcClientError(message);
            }
            default:
                throw DaemonIpcClientError("unexpected IPC frame during session I/O pump");
        }
    }

    void DaemonIpcClient::pumpInputFromStream(std::istream &in) {
        std::array<char, 4096> chunk{};
        while (in.rdbuf()->in_avail() > 0) {
            const std::streamsize toRead =
                std::min(static_cast<std::streamsize>(in.rdbuf()->in_avail()),
                         static_cast<std::streamsize>(kDaemonIpcMaxSessionDataSize));
            in.read(chunk.data(), toRead);
            const std::streamsize got = in.gcount();
            if (got <= 0) {
                break;
            }

            DaemonIpcSessionDataPayload payload{};
            payload.data.assign(reinterpret_cast<const std::uint8_t *>(chunk.data()),
                                reinterpret_cast<const std::uint8_t *>(chunk.data() + got));
            DaemonIpcFrame frame{};
            frame.type = DaemonIpcMessageType::SessionInput;
            frame.payload = encodeSessionDataPayload(payload);
            sendFrame(frame);
        }
    }

    int DaemonIpcClient::runIoPump(std::istream &in, std::ostream &out, std::ostream &err) {
        if (fd_ < 0 || !attachedPort_.has_value()) {
            throw DaemonIpcClientError("session not attached");
        }

        const int inputFd = inputFileDescriptor(in);
        bool inputEof = false;
        int idlePollsAfterEof = 0;

        while (!stopRequested_.load(std::memory_order_acquire)) {
            if (!pendingWrite_.empty()) {
                if (!flushPendingWrite()) {
                    err << "gemini-console: failed to write to daemon\n";
                    return 1;
                }
            }

            pollfd pollfds[2]{};
            nfds_t pollCount = 0;
            pollfds[pollCount].fd = fd_;
            pollfds[pollCount].events = POLLIN | (pendingWrite_.empty() ? 0 : POLLOUT);
            ++pollCount;

            if (inputFd >= 0 && !inputEof) {
                pollfds[pollCount].fd = inputFd;
                pollfds[pollCount].events = POLLIN;
                ++pollCount;
            }

            const int pollResult = ::poll(pollfds, pollCount, 100);
            if (pollResult < 0 && errno != EINTR) {
                err << "gemini-console: poll failed\n";
                return 1;
            }

            if ((pollfds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                return 0;
            }

            bool readSocketFrame = false;

            if ((pollfds[0].revents & POLLOUT) != 0 || !pendingWrite_.empty()) {
                if (!flushPendingWrite()) {
                    err << "gemini-console: failed to write to daemon\n";
                    return 1;
                }
            }

            if ((pollfds[0].revents & POLLIN) != 0) {
                for (;;) {
                    const DaemonIpcTryReadResult result = tryReadDaemonIpcFrame(fd_, readBuffer_);
                    if (result.status == DaemonIpcTryReadStatus::Incomplete) {
                        break;
                    }
                    if (result.status == DaemonIpcTryReadStatus::ConnectionClosed ||
                        result.status == DaemonIpcTryReadStatus::ProtocolError || !result.frame.has_value()) {
                        return 0;
                    }
                    dispatchIncomingFrame(*result.frame, out, err);
                    readSocketFrame = true;
                }
            }

            if (inputFd >= 0 && pollCount > 1 && (pollfds[1].revents & POLLIN) != 0) {
                std::array<std::uint8_t, 4096> chunk{};
                const auto readBytes = ::read(inputFd, chunk.data(), chunk.size());
                if (readBytes == 0) {
                    inputEof = true;
                } else if (readBytes > 0) {
                    std::size_t offset = 0;
                    while (offset < static_cast<std::size_t>(readBytes)) {
                        const std::size_t chunkSize =
                            std::min(static_cast<std::size_t>(readBytes) - offset, kDaemonIpcMaxSessionDataSize);
                        DaemonIpcSessionDataPayload payload{};
                        payload.data.assign(chunk.begin() + static_cast<std::ptrdiff_t>(offset),
                                            chunk.begin() + static_cast<std::ptrdiff_t>(offset + chunkSize));
                        DaemonIpcFrame frame{};
                        frame.type = DaemonIpcMessageType::SessionInput;
                        frame.payload = encodeSessionDataPayload(payload);
                        sendFrame(frame);
                        offset += chunkSize;
                    }
                }
            } else if (inputFd < 0) {
                if (in.rdbuf()->in_avail() > 0) {
                    pumpInputFromStream(in);
                } else if (in.peek() == std::char_traits<char>::eof()) {
                    inputEof = true;
                }
            }

            if (inputEof) {
                if (readSocketFrame || pollResult > 0) {
                    idlePollsAfterEof = 0;
                } else if (pollResult == 0) {
                    ++idlePollsAfterEof;
                    if (idlePollsAfterEof >= 5) {
                        break;
                    }
                }
            }
        }

        return stopRequested_.load(std::memory_order_acquire) ? 130 : 0;
    }
} // namespace PickCore

#else

namespace PickCore {
    DaemonIpcClient::DaemonIpcClient(std::filesystem::path socketPath)
        : socketPath_(std::move(socketPath)) {}

    DaemonIpcClient::~DaemonIpcClient() = default;
} // namespace PickCore

#endif
