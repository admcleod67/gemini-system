//
// Embedded daemon + session table + cooperative runner (Milestone 15 Stage 1).
//

#ifndef PICK_SYSTEM_TCL_GEMINI_SESSION_HOST_H
#define PICK_SYSTEM_TCL_GEMINI_SESSION_HOST_H

#include "AdminQueries.h"
#include "SessionTable.h"

#include <CooperativeSessionRunner.h>
#include <IpcSessionChannel.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

namespace PickShell {
    class GeminiSessionHost {
    public:
        explicit GeminiSessionHost(PickCore::GeminiServiceDaemon &daemon, std::size_t maxSessions = 1);

        [[nodiscard]] SessionHandle createSession();
        void destroySession(PickCore::SessionId id);
        void destroyAllSessions();
        void runExclusive(PickCore::SessionId id, const std::function<void()> &fn);

        void acquire(PickCore::SessionId id);
        void acquireAfterInputWake(PickCore::SessionId id);
        void release(PickCore::SessionId id);
        void yieldWaitingForInput(PickCore::SessionId id);
        void resume(PickCore::SessionId id);
        void retireSession(PickCore::SessionId id);
        [[nodiscard]] PickCore::SessionRunState sessionRunState(PickCore::SessionId id) const;
        [[nodiscard]] std::optional<PickCore::SessionId> activeSession() const;

        void bindIpcChannelScheduling(PickCore::SessionId id, PickCore::IpcSessionChannel &channel);
        void clearIpcChannelScheduling(PickCore::IpcSessionChannel &channel);

        void setConsoleBoundQuery(std::function<bool(PickCore::SessionId)> query);
        void setAdminSocketPath(std::filesystem::path path);

        [[nodiscard]] std::vector<AdminSessionRow> listAdminSessions() const;
        [[nodiscard]] AdminDaemonStatus adminStatus() const;

        [[nodiscard]] PickCore::GeminiServiceDaemon &daemon() noexcept { return daemon_; }
        [[nodiscard]] const PickCore::GeminiServiceDaemon &daemon() const noexcept { return daemon_; }

        [[nodiscard]] SessionTable &sessions() noexcept { return sessions_; }
        [[nodiscard]] const SessionTable &sessions() const noexcept { return sessions_; }

    private:
        PickCore::GeminiServiceDaemon &daemon_;
        SessionTable sessions_;
        PickCore::CooperativeSessionRunner runner_;
        std::function<bool(PickCore::SessionId)> consoleBoundQuery_;
        std::filesystem::path adminSocketPath_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_GEMINI_SESSION_HOST_H
