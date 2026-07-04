#include "DaemonIpcServer.h"

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <stdexcept>

namespace PickCore {
    namespace {
        void closeFd(int &fd) {
            if (fd >= 0) {
                ::close(fd);
                fd = -1;
            }
        }
    } // namespace

    DaemonIpcServer::DaemonIpcServer(std::filesystem::path socketPath) : socketPath_(std::move(socketPath)) {}

    void DaemonIpcServer::start() {
        if (listenFd_ >= 0) {
            return;
        }

        listenFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listenFd_ < 0) {
            throw std::runtime_error("IPC server: failed to create socket");
        }

        std::error_code ec;
        if (socketPath_.has_parent_path()) {
            std::filesystem::create_directories(socketPath_.parent_path(), ec);
        }
        if (std::filesystem::exists(socketPath_, ec)) {
            std::filesystem::remove(socketPath_, ec);
        }

        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        const std::string path = socketPath_.string();
        if (path.size() >= sizeof(address.sun_path)) {
            closeFd(listenFd_);
            throw std::runtime_error("IPC server: socket path too long");
        }
        std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);

        if (::bind(listenFd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
            closeFd(listenFd_);
            throw std::runtime_error("IPC server: bind failed");
        }

        if (::chmod(path.c_str(), S_IRUSR | S_IWUSR) != 0) {
            closeFd(listenFd_);
            std::filesystem::remove(socketPath_, ec);
            throw std::runtime_error("IPC server: failed to set socket permissions");
        }

        if (::listen(listenFd_, 16) != 0) {
            closeFd(listenFd_);
            std::filesystem::remove(socketPath_, ec);
            throw std::runtime_error("IPC server: listen failed");
        }
    }

    void DaemonIpcServer::stop() {
        closeClient();
        closeFd(listenFd_);

        std::error_code ec;
        if (std::filesystem::exists(socketPath_, ec)) {
            std::filesystem::remove(socketPath_, ec);
        }
    }

    bool DaemonIpcServer::pollAccept(const std::chrono::milliseconds timeout) {
        if (listenFd_ < 0) {
            return false;
        }
        closeClient();

        pollfd pfd{};
        pfd.fd = listenFd_;
        pfd.events = POLLIN;
        const int pollResult = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
        if (pollResult <= 0 || (pfd.revents & POLLIN) == 0) {
            return false;
        }

        clientFd_ = ::accept(listenFd_, nullptr, nullptr);
        return clientFd_ >= 0;
    }

    bool DaemonIpcServer::sendError(const int clientFd, const DaemonIpcErrorCode code, const std::string &message) {
        DaemonIpcErrorPayload payload{};
        payload.code = code;
        payload.message = message;
        DaemonIpcFrame frame{};
        frame.type = DaemonIpcMessageType::Error;
        frame.payload = encodeErrorPayload(payload);
        return writeDaemonIpcFrame(clientFd, frame);
    }

    bool DaemonIpcServer::handleConnectedClient(const DaemonIpcServerConfig &config,
                                                const DaemonIpcHandlers &handlers) {
        if (clientFd_ < 0) {
            return false;
        }

        const int activeClient = clientFd_;
        clientFd_ = -1;

        bool handshaken = false;
        bool shutdownRequested = false;

        for (;;) {
            const std::optional<DaemonIpcFrame> frame = readDaemonIpcFrame(activeClient);
            if (!frame.has_value()) {
                break;
            }

            if (frame->version != kDaemonIpcProtocolVersion) {
                sendError(activeClient, DaemonIpcErrorCode::UnsupportedVersion, "unsupported protocol version");
                break;
            }

            if (!handshaken) {
                if (frame->type != DaemonIpcMessageType::Handshake) {
                    sendError(activeClient, DaemonIpcErrorCode::NotHandshaken, "handshake required");
                    break;
                }

                const std::optional<DaemonIpcHandshakePayload> handshake = decodeHandshakePayload(frame->payload);
                if (!handshake.has_value() || handshake->clientProtocolVersion != kDaemonIpcProtocolVersion) {
                    sendError(activeClient, DaemonIpcErrorCode::UnsupportedVersion, "unsupported client protocol version");
                    break;
                }

                DaemonIpcHandshakeAckPayload ackPayload{};
                ackPayload.serverProtocolVersion = kDaemonIpcProtocolVersion;
                ackPayload.maxSessions = static_cast<std::uint16_t>(config.maxSessions);
                ackPayload.buildVersion = config.buildVersion;
                DaemonIpcFrame ackFrame{};
                ackFrame.type = DaemonIpcMessageType::HandshakeAck;
                ackFrame.payload = encodeHandshakeAckPayload(ackPayload);
                if (!writeDaemonIpcFrame(activeClient, ackFrame)) {
                    break;
                }
                handshaken = true;
                continue;
            }

            switch (frame->type) {
                case DaemonIpcMessageType::Ping: {
                    DaemonIpcFrame pong{};
                    pong.type = DaemonIpcMessageType::Pong;
                    if (!writeDaemonIpcFrame(activeClient, pong)) {
                        close(activeClient);
                        return shutdownRequested;
                    }
                    continue;
                }
                case DaemonIpcMessageType::ShutdownRequest: {
                    DaemonIpcFrame ack{};
                    ack.type = DaemonIpcMessageType::ShutdownAck;
                    if (!writeDaemonIpcFrame(activeClient, ack)) {
                        break;
                    }
                    if (handlers.requestShutdown) {
                        handlers.requestShutdown();
                    }
                    shutdownRequested = true;
                    break;
                }
                case DaemonIpcMessageType::ReserveSession: {
                    if (!handlers.reserveSession) {
                        sendError(activeClient, DaemonIpcErrorCode::InvalidMessage, "reserve session unavailable");
                        break;
                    }
                    const std::optional<SessionId> sessionId = handlers.reserveSession();
                    if (!sessionId.has_value()) {
                        sendError(activeClient, DaemonIpcErrorCode::SessionTableFull, "session table full");
                        break;
                    }
                    DaemonIpcReserveSessionAckPayload ackPayload{};
                    ackPayload.sessionPort = *sessionId;
                    DaemonIpcFrame ack{};
                    ack.type = DaemonIpcMessageType::ReserveSessionAck;
                    ack.payload = encodeReserveSessionAckPayload(ackPayload);
                    if (!writeDaemonIpcFrame(activeClient, ack)) {
                        break;
                    }
                    continue;
                }
                default:
                    sendError(activeClient, DaemonIpcErrorCode::InvalidMessage, "invalid message type");
                    break;
            }
            break;
        }

        close(activeClient);
        return shutdownRequested;
    }

    void DaemonIpcServer::closeClient() {
        closeFd(clientFd_);
    }
} // namespace PickCore
