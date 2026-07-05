//
// IPC-backed session I/O streams for gemini-daemon (Milestone 14 Stage 3).
//

#ifndef PICK_SYSTEM_CORE_DAEMON_IPC_SESSION_CHANNEL_H
#define PICK_SYSTEM_CORE_DAEMON_IPC_SESSION_CHANNEL_H

#include "DaemonIpcProtocol.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <istream>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <span>
#include <streambuf>
#include <vector>

namespace PickCore {
    class ChannelInputBuffer;
    class ChannelOutputBuffer;

    struct SessionInputScheduling {
        std::function<void()> yieldBeforeBlockingRead;
        std::function<void()> acquireAfterWake;
        std::function<void()> resumeOnInput;
    };

    class IpcSessionChannel {
    public:
        IpcSessionChannel();
        ~IpcSessionChannel();

        IpcSessionChannel(const IpcSessionChannel &) = delete;
        IpcSessionChannel &operator=(const IpcSessionChannel &) = delete;

        [[nodiscard]] std::istream &input();
        [[nodiscard]] std::ostream &output();
        [[nodiscard]] std::ostream &diagnostic();

        void pushInput(std::span<const std::uint8_t> bytes);
        void close();

        void setSessionScheduling(SessionInputScheduling scheduling);
        void clearSessionScheduling();
        [[nodiscard]] bool hasSessionScheduling() const;

        void flushPendingOutput(int clientFd);
        [[nodiscard]] bool hasPendingOutput() const;

    private:
        friend class ChannelInputBuffer;
        friend class ChannelOutputBuffer;

        int readInputChar();
        void writeOutputChar(int ch, bool diagnostic);
        void writeOutputBytes(const char *data, std::streamsize count, bool diagnostic);
        void markPendingOutput();
        bool flushQueue(int clientFd, DaemonIpcMessageType type, std::vector<std::uint8_t> &queue);

        mutable std::mutex mutex_;
        std::condition_variable inputAvailable_;
        std::deque<std::uint8_t> inputQueue_;
        std::vector<std::uint8_t> outputQueue_;
        std::vector<std::uint8_t> diagnosticQueue_;
        bool closed_{false};
        bool hasPendingOutput_{false};
        std::optional<SessionInputScheduling> scheduling_;

        std::unique_ptr<std::streambuf> inputBuffer_;
        std::unique_ptr<std::streambuf> outputBuffer_;
        std::unique_ptr<std::streambuf> diagnosticBuffer_;

        std::istream inputStream_;
        std::ostream outputStream_;
        std::ostream diagnosticStream_;
    };
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_IPC_SESSION_CHANNEL_H
