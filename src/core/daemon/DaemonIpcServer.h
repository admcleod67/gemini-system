//
// Unix domain socket IPC server for Gemini daemon (Milestone 13 Stage 5, M14 multi-client).
//

#ifndef PICK_SYSTEM_CORE_DAEMON_DAEMON_IPC_SERVER_H
#define PICK_SYSTEM_CORE_DAEMON_DAEMON_IPC_SERVER_H

#include "DaemonIpcProtocol.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace PickCore {
    struct DaemonIpcHandlers {
        std::function<std::optional<SessionId>()> reserveSession;
        std::function<void()> requestShutdown;
    };

    struct DaemonIpcServerConfig {
        std::size_t maxSessions{64};
        std::string buildVersion;
    };

    class DaemonIpcServer {
    public:
        explicit DaemonIpcServer(std::filesystem::path socketPath);

        void start();
        void stop();

        [[nodiscard]] bool isListening() const { return listenFd_ >= 0; }
        [[nodiscard]] bool pollAndDispatch(std::chrono::milliseconds timeout,
                                           const DaemonIpcServerConfig &config,
                                           const DaemonIpcHandlers &handlers);

    private:
        struct Connection {
            int fd{-1};
            bool handshaken{false};
            std::vector<std::uint8_t> readBuffer;
        };

        struct DispatchOutcome {
            bool closeConnection{false};
            bool shutdownRequested{false};
        };

        void acceptPendingConnections();
        void removeConnectionAt(std::size_t index);
        void closeAllConnections();
        [[nodiscard]] bool sendError(int clientFd, DaemonIpcErrorCode code, const std::string &message);
        [[nodiscard]] DispatchOutcome dispatchFrame(Connection &connection,
                                                    const DaemonIpcFrame &frame,
                                                    const DaemonIpcServerConfig &config,
                                                    const DaemonIpcHandlers &handlers);

        std::filesystem::path socketPath_;
        int listenFd_{-1};
        std::vector<Connection> connections_;
    };
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_DAEMON_IPC_SERVER_H
