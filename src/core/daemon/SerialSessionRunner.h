//
// Exclusive execution token for one session at a time (Milestone 13 Stage 2).
//

#ifndef PICK_SYSTEM_CORE_DAEMON_SERIAL_SESSION_RUNNER_H
#define PICK_SYSTEM_CORE_DAEMON_SERIAL_SESSION_RUNNER_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>

namespace PickCore {
    using SessionId = std::uint32_t;

    class SerialSessionRunner {
    public:
        void runExclusive(SessionId id, const std::function<void()> &fn);

        [[nodiscard]] std::optional<SessionId> activeSession() const;

    private:
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::optional<SessionId> active_;
        std::uint32_t depth_{0};
    };
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_SERIAL_SESSION_RUNNER_H
