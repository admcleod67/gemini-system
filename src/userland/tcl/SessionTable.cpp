#include "SessionTable.h"

#include "GeminiSession.h"

namespace PickShell {
    SessionTable::SessionTable(const std::size_t maxSessions) : maxSessions_(maxSessions < 1 ? 1 : maxSessions) {}

    SessionHandle SessionTable::createSession(PickCore::GeminiServiceDaemon &daemon) {
        if (sessions_.size() >= maxSessions_) {
            throw std::runtime_error("session table full");
        }

        const PickCore::SessionId id = nextId_++;
        std::unique_ptr<GeminiSession> session = GeminiSession::create();
        session->setSharedLockTable(daemon.lockTable());
        session->runtime().setLanguageRegistry(&daemon.languageRegistry());

        GeminiSession &ref = *session;
        sessions_.emplace(id, std::move(session));
        return SessionHandle{id, ref};
    }

    void SessionTable::destroySession(const PickCore::SessionId id) {
        const auto it = sessions_.find(id);
        if (it == sessions_.end()) {
            return;
        }
        it->second->destroy();
        sessions_.erase(it);
    }

    GeminiSession *SessionTable::find(const PickCore::SessionId id) {
        const auto it = sessions_.find(id);
        if (it == sessions_.end()) {
            return nullptr;
        }
        return it->second.get();
    }
} // namespace PickShell
