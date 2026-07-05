#include "GeminiSessionHost.h"

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
} // namespace PickShell
