//
// Cooperative session execution token (Milestone 15 Stage 1).
//

#ifndef PICK_SYSTEM_CORE_DAEMON_COOPERATIVE_SESSION_RUNNER_H
#define PICK_SYSTEM_CORE_DAEMON_COOPERATIVE_SESSION_RUNNER_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace PickCore {
    using SessionId = std::uint32_t;

    enum class SessionRunState {
        Runnable,
        Running,
        WaitingForInput,
    };

    class CooperativeSessionRunner {
    public:
        void acquire(SessionId id);
        void acquireAfterInputWake(SessionId id);
        void release(SessionId id);
        void yieldWaitingForInput(SessionId id);
        void resume(SessionId id);
        void retireSession(SessionId id);

        void runExclusive(SessionId id, const std::function<void()> &fn);

        [[nodiscard]] std::optional<SessionId> activeSession() const;
        [[nodiscard]] SessionRunState state(SessionId id) const;
        [[nodiscard]] std::optional<SessionId> nextRunnableSession() const;

    private:
        [[nodiscard]] SessionRunState stateLocked(SessionId id) const;
        [[nodiscard]] std::optional<SessionId> nextRunnableSessionLocked(
            const std::optional<SessionId> requester = std::nullopt) const;
        [[nodiscard]] bool canAcquireLocked(SessionId id) const;
        [[nodiscard]] bool canAcquireAfterInputWakeLocked(SessionId id) const;
        void grantAcquireLocked(SessionId id);

        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::optional<SessionId> active_;
        std::uint32_t depth_{0};
        SessionId lastServed_{0};
        std::unordered_map<SessionId, SessionRunState> sessionStates_;
    };
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_COOPERATIVE_SESSION_RUNNER_H
