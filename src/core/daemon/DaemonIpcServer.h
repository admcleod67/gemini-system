//
// Unix domain socket IPC server for Gemini daemon (Milestone 13 Stage 5).
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
        [[nodiscard]] bool pollAccept(std::chrono::milliseconds timeout);
        bool handleConnectedClient(const DaemonIpcServerConfig &config, const DaemonIpcHandlers &handlers);

    private:
        void closeClient();
        bool sendError(int clientFd, DaemonIpcErrorCode code, const std::string &message);

        std::filesystem::path socketPath_;
        int listenFd_{-1};
        int clientFd_{-1};
    };
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_DAEMON_IPC_SERVER_H
