//
// Daemon session table: owns GeminiSession objects (Milestone 13 Stage 2).
//

#ifndef PICK_SYSTEM_TCL_SESSION_TABLE_H
#define PICK_SYSTEM_TCL_SESSION_TABLE_H

#include <GeminiServiceDaemon.h>
#include <CooperativeSessionRunner.h>

#include "GeminiSession.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace PickShell {
    struct SessionHandle {
        PickCore::SessionId id{0};
        GeminiSession &session;
    };

    class SessionTable {
    public:
        explicit SessionTable(std::size_t maxSessions = 1);

        [[nodiscard]] SessionHandle createSession(PickCore::GeminiServiceDaemon &daemon);
        void destroySession(PickCore::SessionId id);

        [[nodiscard]] GeminiSession *find(PickCore::SessionId id);
        [[nodiscard]] const GeminiSession *find(PickCore::SessionId id) const;
        [[nodiscard]] std::vector<PickCore::SessionId> sessionIds() const;
        [[nodiscard]] std::size_t count() const { return sessions_.size(); }
        [[nodiscard]] std::size_t maxSessions() const { return maxSessions_; }

    private:
        PickCore::GeminiServiceDaemon *daemon_{nullptr};
        std::size_t maxSessions_;
        std::unordered_map<PickCore::SessionId, std::unique_ptr<GeminiSession>> sessions_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_SESSION_TABLE_H
