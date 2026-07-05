#include "CooperativeSessionRunner.h"

#include <algorithm>
#include <condition_variable>
#include <stdexcept>
#include <vector>

namespace PickCore {
    SessionRunState CooperativeSessionRunner::stateLocked(const SessionId id) const {
        const auto it = sessionStates_.find(id);
        if (it == sessionStates_.end()) {
            return SessionRunState::Runnable;
        }
        return it->second;
    }

    std::optional<SessionId> CooperativeSessionRunner::nextRunnableSessionLocked(
        const std::optional<SessionId> requester) const {
        std::vector<SessionId> runnable;
        runnable.reserve(sessionStates_.size() + (requester.has_value() ? 1U : 0U));
        for (const auto &[id, state] : sessionStates_) {
            if (state == SessionRunState::Runnable) {
                runnable.push_back(id);
            }
        }
        if (requester.has_value() && stateLocked(*requester) == SessionRunState::Runnable &&
            std::find(runnable.begin(), runnable.end(), *requester) == runnable.end()) {
            runnable.push_back(*requester);
        }
        if (runnable.empty()) {
            return std::nullopt;
        }

        std::sort(runnable.begin(), runnable.end());
        for (const SessionId id : runnable) {
            if (id > lastServed_) {
                return id;
            }
        }
        return runnable.front();
    }

    bool CooperativeSessionRunner::canAcquireLocked(const SessionId id) const {
        if (active_.has_value() && *active_ == id) {
            return true;
        }
        if (active_.has_value() || stateLocked(id) == SessionRunState::WaitingForInput) {
            return false;
        }

        const std::optional<SessionId> next = nextRunnableSessionLocked(id);
        return !next.has_value() || *next == id;
    }

    bool CooperativeSessionRunner::canAcquireAfterInputWakeLocked(const SessionId id) const {
        if (active_.has_value() && *active_ == id) {
            return true;
        }
        return !active_.has_value() && stateLocked(id) != SessionRunState::WaitingForInput;
    }

    void CooperativeSessionRunner::grantAcquireLocked(const SessionId id) {
        if (!active_.has_value()) {
            active_ = id;
            depth_ = 0;
        }
        if (!active_.has_value() || *active_ != id) {
            throw std::logic_error("cooperative session acquire failed");
        }

        ++depth_;
        sessionStates_[id] = SessionRunState::Running;
    }

    void CooperativeSessionRunner::acquire(const SessionId id) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&] { return canAcquireLocked(id); });
        grantAcquireLocked(id);
    }

    void CooperativeSessionRunner::acquireAfterInputWake(const SessionId id) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&] { return canAcquireAfterInputWakeLocked(id); });
        grantAcquireLocked(id);
    }

    void CooperativeSessionRunner::release(const SessionId id) {
        const std::lock_guard lock(mutex_);
        if (!active_.has_value() || *active_ != id) {
            throw std::logic_error("release from non-holder session");
        }
        if (depth_ == 0) {
            throw std::logic_error("release with zero depth");
        }

        --depth_;
        if (depth_ == 0) {
            lastServed_ = id;
            active_.reset();
            sessionStates_[id] = SessionRunState::Runnable;
            cv_.notify_all();
        }
    }

    void CooperativeSessionRunner::yieldWaitingForInput(const SessionId id) {
        const std::lock_guard lock(mutex_);
        if (!active_.has_value() || *active_ != id || depth_ == 0) {
            throw std::logic_error("yield from non-holder session");
        }

        lastServed_ = id;
        sessionStates_[id] = SessionRunState::WaitingForInput;
        depth_ = 0;
        active_.reset();
        cv_.notify_all();
    }

    void CooperativeSessionRunner::resume(const SessionId id) {
        const std::lock_guard lock(mutex_);
        sessionStates_[id] = SessionRunState::Runnable;
        cv_.notify_all();
    }

    void CooperativeSessionRunner::retireSession(const SessionId id) {
        const std::lock_guard lock(mutex_);
        sessionStates_.erase(id);
        if (active_.has_value() && *active_ == id) {
            lastServed_ = id;
            active_.reset();
            depth_ = 0;
        }
        cv_.notify_all();
    }

    void CooperativeSessionRunner::runExclusive(const SessionId id, const std::function<void()> &fn) {
        acquire(id);
        fn();
        const std::lock_guard lock(mutex_);
        if (active_.has_value() && *active_ == id && depth_ > 0) {
            --depth_;
            if (depth_ == 0) {
                lastServed_ = id;
                active_.reset();
                sessionStates_[id] = SessionRunState::Runnable;
                cv_.notify_all();
            }
        } else if (!active_.has_value() && stateLocked(id) == SessionRunState::WaitingForInput) {
            sessionStates_[id] = SessionRunState::Runnable;
            cv_.notify_all();
        }
    }

    std::optional<SessionId> CooperativeSessionRunner::activeSession() const {
        const std::lock_guard lock(mutex_);
        return active_;
    }

    SessionRunState CooperativeSessionRunner::state(const SessionId id) const {
        const std::lock_guard lock(mutex_);
        return stateLocked(id);
    }

    std::optional<SessionId> CooperativeSessionRunner::nextRunnableSession() const {
        const std::lock_guard lock(mutex_);
        return nextRunnableSessionLocked();
    }
} // namespace PickCore
