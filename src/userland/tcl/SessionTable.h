//
// Daemon session table: owns GeminiSession objects (Milestone 13 Stage 2).
//

#ifndef PICK_SYSTEM_TCL_SESSION_TABLE_H
#define PICK_SYSTEM_TCL_SESSION_TABLE_H

#include <GeminiServiceDaemon.h>
#include <SerialSessionRunner.h>

#include "GeminiSession.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <unordered_map>

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
        [[nodiscard]] std::size_t count() const { return sessions_.size(); }
        [[nodiscard]] std::size_t maxSessions() const { return maxSessions_; }

    private:
        std::size_t maxSessions_;
        PickCore::SessionId nextId_{1};
        std::unordered_map<PickCore::SessionId, std::unique_ptr<GeminiSession>> sessions_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_SESSION_TABLE_H
