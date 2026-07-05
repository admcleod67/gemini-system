//
// Gemini daemon IPC client for gemini-console (Milestone 14 Stage 4).
//

#ifndef PICK_SYSTEM_CORE_DAEMON_DAEMON_IPC_CLIENT_H
#define PICK_SYSTEM_CORE_DAEMON_DAEMON_IPC_CLIENT_H

#include "DaemonIpcProtocol.h"

#include <atomic>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <vector>

namespace PickCore {
    class DaemonIpcClientError : public std::runtime_error {
    public:
        explicit DaemonIpcClientError(const std::string &message) : std::runtime_error(message) {}
    };

    class DaemonIpcClient {
    public:
        explicit DaemonIpcClient(std::filesystem::path socketPath);
        ~DaemonIpcClient();

        DaemonIpcClient(const DaemonIpcClient &) = delete;
        DaemonIpcClient &operator=(const DaemonIpcClient &) = delete;

#ifndef _WIN32
        void connect();
        void disconnect();
        void handshake();
        SessionId attachSession(SessionId requestedPort = 0);
        void detachSession();

        int runIoPump(std::istream &in, std::ostream &out, std::ostream &err);

        void requestStop() { stopRequested_.store(true, std::memory_order_release); }

        [[nodiscard]] bool isConnected() const { return fd_ >= 0; }
        [[nodiscard]] std::optional<SessionId> attachedPort() const { return attachedPort_; }

    private:
        void sendFrame(const DaemonIpcFrame &frame);
        DaemonIpcFrame recvFrameBlocking();
        bool flushPendingWrite();
        void dispatchIncomingFrame(const DaemonIpcFrame &frame, std::ostream &out, std::ostream &err);
        void pumpInputFromStream(std::istream &in);
        void closeFd();

        int fd_{-1};
        std::vector<std::uint8_t> readBuffer_;
        std::vector<std::uint8_t> pendingWrite_;
        std::optional<SessionId> attachedPort_;
        std::atomic<bool> stopRequested_{false};
#endif

        std::filesystem::path socketPath_;
    };
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_DAEMON_IPC_CLIENT_H
