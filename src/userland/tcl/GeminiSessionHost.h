//
// Embedded daemon + session table + serial runner (Milestone 13 Stage 2).
//

#ifndef PICK_SYSTEM_TCL_GEMINI_SESSION_HOST_H
#define PICK_SYSTEM_TCL_GEMINI_SESSION_HOST_H

#include "SessionTable.h"

#include <functional>

namespace PickShell {
    class GeminiSessionHost {
    public:
        explicit GeminiSessionHost(PickCore::GeminiServiceDaemon &daemon, std::size_t maxSessions = 1);

        [[nodiscard]] SessionHandle createSession();
        void destroySession(PickCore::SessionId id);
        void runExclusive(PickCore::SessionId id, const std::function<void()> &fn);

        [[nodiscard]] PickCore::GeminiServiceDaemon &daemon() noexcept { return daemon_; }
        [[nodiscard]] const PickCore::GeminiServiceDaemon &daemon() const noexcept { return daemon_; }

        [[nodiscard]] SessionTable &sessions() noexcept { return sessions_; }
        [[nodiscard]] const SessionTable &sessions() const noexcept { return sessions_; }

    private:
        PickCore::GeminiServiceDaemon &daemon_;
        SessionTable sessions_;
        PickCore::SerialSessionRunner runner_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_GEMINI_SESSION_HOST_H
