#include "SessionTable.h"

#include "GeminiSession.h"

#include <algorithm>

namespace PickShell {
    SessionTable::SessionTable(const std::size_t maxSessions) : maxSessions_(maxSessions < 1 ? 1 : maxSessions) {}

    SessionHandle SessionTable::createSession(PickCore::GeminiServiceDaemon &daemon) {
        if (sessions_.size() >= maxSessions_) {
            throw std::runtime_error("session table full");
        }

        daemon_ = &daemon;
        const std::optional<PickCore::SessionId> port = daemon.portManager().allocate();
        if (!port.has_value()) {
            throw std::runtime_error("session table full");
        }

        std::unique_ptr<GeminiSession> session = GeminiSession::create();
        session->setSharedLockTable(daemon.lockTable());
        session->runtime().setLanguageRegistry(&daemon.languageRegistry());
        session->setDaemonPort(static_cast<int>(*port));

        GeminiSession &ref = *session;
        sessions_.emplace(*port, std::move(session));
        return SessionHandle{*port, ref};
    }

    void SessionTable::destroySession(const PickCore::SessionId id) {
        const auto it = sessions_.find(id);
        if (it == sessions_.end()) {
            return;
        }
        it->second->destroy();
        sessions_.erase(it);
        if (daemon_ != nullptr) {
            daemon_->portManager().release(id);
        }
    }

    GeminiSession *SessionTable::find(const PickCore::SessionId id) {
        const auto it = sessions_.find(id);
        if (it == sessions_.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    const GeminiSession *SessionTable::find(const PickCore::SessionId id) const {
        const auto it = sessions_.find(id);
        if (it == sessions_.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    std::vector<PickCore::SessionId> SessionTable::sessionIds() const {
        std::vector<PickCore::SessionId> ids;
        ids.reserve(sessions_.size());
        for (const auto &[id, session]: sessions_) {
            (void) session;
            ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }
} // namespace PickShell
