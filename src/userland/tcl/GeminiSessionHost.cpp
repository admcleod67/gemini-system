#include "GeminiSessionHost.h"

#include <pick_system/version.hpp>

#include <algorithm>
#include <vector>

namespace PickShell {
    GeminiSessionHost::GeminiSessionHost(PickCore::GeminiServiceDaemon &daemon, const std::size_t maxSessions)
        : daemon_(daemon), sessions_(maxSessions) {}

    SessionHandle GeminiSessionHost::createSession() {
        return sessions_.createSession(daemon_);
    }

    void GeminiSessionHost::destroySession(const PickCore::SessionId id) {
        sessions_.destroySession(id);
    }

    void GeminiSessionHost::destroyAllSessions() {
        const std::vector<PickCore::SessionId> ids = sessions_.sessionIds();
        for (const PickCore::SessionId id: ids) {
            sessions_.destroySession(id);
        }
    }

    void GeminiSessionHost::runExclusive(const PickCore::SessionId id, const std::function<void()> &fn) {
        runner_.runExclusive(id, fn);
    }

    void GeminiSessionHost::acquire(const PickCore::SessionId id) {
        runner_.acquire(id);
    }

    void GeminiSessionHost::acquireAfterInputWake(const PickCore::SessionId id) {
        runner_.acquireAfterInputWake(id);
    }

    void GeminiSessionHost::release(const PickCore::SessionId id) {
        runner_.release(id);
    }

    void GeminiSessionHost::yieldWaitingForInput(const PickCore::SessionId id) {
        runner_.yieldWaitingForInput(id);
    }

    void GeminiSessionHost::resume(const PickCore::SessionId id) {
        runner_.resume(id);
    }

    void GeminiSessionHost::retireSession(const PickCore::SessionId id) {
        runner_.retireSession(id);
    }

    PickCore::SessionRunState GeminiSessionHost::sessionRunState(const PickCore::SessionId id) const {
        return runner_.state(id);
    }

    void GeminiSessionHost::bindIpcChannelScheduling(const PickCore::SessionId id,
                                                     PickCore::IpcSessionChannel &channel) {
        channel.setSessionScheduling(PickCore::SessionInputScheduling{
            [this, id] { yieldWaitingForInput(id); },
            [this, id] { acquireAfterInputWake(id); },
            [this, id] { resume(id); },
        });
    }

    void GeminiSessionHost::clearIpcChannelScheduling(PickCore::IpcSessionChannel &channel) {
        channel.clearSessionScheduling();
    }

    void GeminiSessionHost::setConsoleBoundQuery(std::function<bool(PickCore::SessionId)> query) {
        consoleBoundQuery_ = std::move(query);
    }

    void GeminiSessionHost::setAdminSocketPath(std::filesystem::path path) {
        adminSocketPath_ = std::move(path);
    }

    std::vector<AdminSessionRow> GeminiSessionHost::listAdminSessions() const {
        std::vector<PickCore::SessionId> ids = sessions_.sessionIds();
        std::sort(ids.begin(), ids.end());

        std::vector<AdminSessionRow> rows;
        rows.reserve(ids.size());
        for (const PickCore::SessionId id : ids) {
            const GeminiSession *session = sessions_.find(id);
            if (session == nullptr) {
                continue;
            }

            AdminSessionRow row;
            row.port = id;
            row.consoleBound = consoleBoundQuery_ ? consoleBoundQuery_(id) : false;
            row.loggedIn = session->loggedIn();
            if (row.loggedIn) {
                row.username = session->sessionUsername();
                row.account = session->sessionAccount();
            }
            row.runState = runner_.state(id);
            rows.push_back(std::move(row));
        }
        return rows;
    }

    AdminDaemonStatus GeminiSessionHost::adminStatus() const {
        AdminDaemonStatus status;
        status.maxSessions = sessions_.maxSessions();
        status.sessionCount = sessions_.count();
        status.socketPath = adminSocketPath_;
        status.version = pick_system::version_string;
        return status;
    }
} // namespace PickShell
