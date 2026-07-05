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

    void GeminiSessionHost::release(const PickCore::SessionId id) {
        runner_.release(id);
    }

    void GeminiSessionHost::yieldWaitingForInput(const PickCore::SessionId id) {
        runner_.yieldWaitingForInput(id);
    }

    void GeminiSessionHost::resume(const PickCore::SessionId id) {
        runner_.resume(id);
    }

    PickCore::SessionRunState GeminiSessionHost::sessionRunState(const PickCore::SessionId id) const {
        return runner_.state(id);
    }
} // namespace PickShell
