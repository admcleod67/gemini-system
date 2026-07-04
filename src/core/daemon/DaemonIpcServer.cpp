#include "DaemonIpcServer.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
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

        void setNonBlocking(const int fd) {
            const int flags = ::fcntl(fd, F_GETFL, 0);
            if (flags >= 0) {
                (void) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }
        }

        [[nodiscard]] bool isSessionPlaneMessage(const DaemonIpcMessageType type) {
            switch (type) {
                case DaemonIpcMessageType::AttachSession:
                case DaemonIpcMessageType::AttachSessionAck:
                case DaemonIpcMessageType::DetachSession:
                case DaemonIpcMessageType::DetachSessionAck:
                case DaemonIpcMessageType::SessionInput:
                case DaemonIpcMessageType::SessionOutput:
                case DaemonIpcMessageType::SessionDiagnostic:
                    return true;
                default:
                    return false;
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

        setNonBlocking(listenFd_);
    }

    void DaemonIpcServer::stop() {
        closeAllConnections();
        closeFd(listenFd_);

        std::error_code ec;
        if (std::filesystem::exists(socketPath_, ec)) {
            std::filesystem::remove(socketPath_, ec);
        }
    }

    void DaemonIpcServer::closeAllConnections() {
        for (Connection &connection : connections_) {
            closeFd(connection.fd);
        }
        connections_.clear();
    }

    void DaemonIpcServer::removeConnectionAt(const std::size_t index) {
        if (index >= connections_.size()) {
            return;
        }
        closeFd(connections_[index].fd);
        connections_.erase(connections_.begin() + static_cast<std::ptrdiff_t>(index));
    }

    void DaemonIpcServer::acceptPendingConnections() {
        if (listenFd_ < 0) {
            return;
        }

        for (;;) {
            const int clientFd = ::accept(listenFd_, nullptr, nullptr);
            if (clientFd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                break;
            }
            setNonBlocking(clientFd);
            connections_.push_back(Connection{clientFd, false, {}});
        }
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

    DaemonIpcServer::DispatchOutcome DaemonIpcServer::dispatchFrame(Connection &connection,
                                                                    const DaemonIpcFrame &frame,
                                                                    const DaemonIpcServerConfig &config,
                                                                    const DaemonIpcHandlers &handlers) {
        DispatchOutcome outcome{};

        if (frame.version != kDaemonIpcProtocolVersion) {
            (void) sendError(connection.fd, DaemonIpcErrorCode::UnsupportedVersion, "unsupported protocol version");
            outcome.closeConnection = true;
            return outcome;
        }

        if (!connection.handshaken) {
            if (frame.type != DaemonIpcMessageType::Handshake) {
                (void) sendError(connection.fd, DaemonIpcErrorCode::NotHandshaken, "handshake required");
                outcome.closeConnection = true;
                return outcome;
            }

            const std::optional<DaemonIpcHandshakePayload> handshake = decodeHandshakePayload(frame.payload);
            if (!handshake.has_value() || handshake->clientProtocolVersion != kDaemonIpcProtocolVersion) {
                (void) sendError(connection.fd, DaemonIpcErrorCode::UnsupportedVersion,
                                "unsupported client protocol version");
                outcome.closeConnection = true;
                return outcome;
            }

            DaemonIpcHandshakeAckPayload ackPayload{};
            ackPayload.serverProtocolVersion = kDaemonIpcProtocolVersion;
            ackPayload.maxSessions = static_cast<std::uint16_t>(config.maxSessions);
            ackPayload.buildVersion = config.buildVersion;
            DaemonIpcFrame ackFrame{};
            ackFrame.type = DaemonIpcMessageType::HandshakeAck;
            ackFrame.payload = encodeHandshakeAckPayload(ackPayload);
            if (!writeDaemonIpcFrame(connection.fd, ackFrame)) {
                outcome.closeConnection = true;
                return outcome;
            }
            connection.handshaken = true;
            return outcome;
        }

        switch (frame.type) {
            case DaemonIpcMessageType::Ping: {
                DaemonIpcFrame pong{};
                pong.type = DaemonIpcMessageType::Pong;
                if (!writeDaemonIpcFrame(connection.fd, pong)) {
                    outcome.closeConnection = true;
                }
                return outcome;
            }
            case DaemonIpcMessageType::ShutdownRequest: {
                DaemonIpcFrame ack{};
                ack.type = DaemonIpcMessageType::ShutdownAck;
                if (!writeDaemonIpcFrame(connection.fd, ack)) {
                    outcome.closeConnection = true;
                    return outcome;
                }
                if (handlers.requestShutdown) {
                    handlers.requestShutdown();
                }
                outcome.shutdownRequested = true;
                outcome.closeConnection = true;
                return outcome;
            }
            case DaemonIpcMessageType::ReserveSession: {
                if (!handlers.reserveSession) {
                    (void) sendError(connection.fd, DaemonIpcErrorCode::InvalidMessage, "reserve session unavailable");
                    outcome.closeConnection = true;
                    return outcome;
                }
                const std::optional<SessionId> sessionId = handlers.reserveSession();
                if (!sessionId.has_value()) {
                    (void) sendError(connection.fd, DaemonIpcErrorCode::SessionTableFull, "session table full");
                    outcome.closeConnection = true;
                    return outcome;
                }
                DaemonIpcReserveSessionAckPayload ackPayload{};
                ackPayload.sessionPort = *sessionId;
                DaemonIpcFrame ack{};
                ack.type = DaemonIpcMessageType::ReserveSessionAck;
                ack.payload = encodeReserveSessionAckPayload(ackPayload);
                if (!writeDaemonIpcFrame(connection.fd, ack)) {
                    outcome.closeConnection = true;
                }
                return outcome;
            }
            default:
                break;
        }

        if (isSessionPlaneMessage(frame.type)) {
            const DaemonIpcErrorCode code = frame.type == DaemonIpcMessageType::SessionInput ||
                                                    frame.type == DaemonIpcMessageType::SessionOutput ||
                                                    frame.type == DaemonIpcMessageType::SessionDiagnostic
                                                ? DaemonIpcErrorCode::NotAttached
                                                : DaemonIpcErrorCode::InvalidMessage;
            const char *message = code == DaemonIpcErrorCode::NotAttached ? "session not attached"
                                                                          : "session attach not available";
            if (!sendError(connection.fd, code, message)) {
                outcome.closeConnection = true;
            }
            return outcome;
        }

        (void) sendError(connection.fd, DaemonIpcErrorCode::InvalidMessage, "invalid message type");
        outcome.closeConnection = true;
        return outcome;
    }

    bool DaemonIpcServer::pollAndDispatch(const std::chrono::milliseconds timeout,
                                          const DaemonIpcServerConfig &config,
                                          const DaemonIpcHandlers &handlers) {
        if (listenFd_ < 0) {
            return false;
        }

        std::vector<pollfd> pollfds;
        pollfds.reserve(1 + connections_.size());
        pollfd listenPoll{};
        listenPoll.fd = listenFd_;
        listenPoll.events = POLLIN;
        pollfds.push_back(listenPoll);

        for (const Connection &connection : connections_) {
            pollfd clientPoll{};
            clientPoll.fd = connection.fd;
            clientPoll.events = POLLIN;
            pollfds.push_back(clientPoll);
        }

        const int pollResult = ::poll(pollfds.data(), static_cast<nfds_t>(pollfds.size()), static_cast<int>(timeout.count()));
        if (pollResult <= 0) {
            return false;
        }

        bool shutdownRequested = false;

        if ((pollfds[0].revents & POLLIN) != 0) {
            acceptPendingConnections();
        }

        std::size_t index = 0;
        while (index < connections_.size()) {
            Connection &connection = connections_[index];

            short revents = 0;
            for (const pollfd &clientPoll : pollfds) {
                if (clientPoll.fd == connection.fd) {
                    revents = clientPoll.revents;
                    break;
                }
            }

            if ((revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                removeConnectionAt(index);
                continue;
            }

            if ((revents & POLLIN) == 0) {
                ++index;
                continue;
            }

            bool closeConnection = false;
            for (;;) {
                const DaemonIpcTryReadResult readResult = tryReadDaemonIpcFrame(connection.fd, connection.readBuffer);
                if (readResult.status == DaemonIpcTryReadStatus::Incomplete) {
                    break;
                }
                if (readResult.status == DaemonIpcTryReadStatus::ConnectionClosed ||
                    readResult.status == DaemonIpcTryReadStatus::ProtocolError || !readResult.frame.has_value()) {
                    closeConnection = true;
                    break;
                }

                const DispatchOutcome outcome =
                    dispatchFrame(connection, *readResult.frame, config, handlers);
                if (outcome.shutdownRequested) {
                    shutdownRequested = true;
                }
                if (outcome.closeConnection) {
                    closeConnection = true;
                    break;
                }
            }

            if (closeConnection) {
                removeConnectionAt(index);
                continue;
            }

            ++index;
        }

        return shutdownRequested;
    }
} // namespace PickCore
