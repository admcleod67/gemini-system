#include "CooperativeSessionRunner.h"

#include <condition_variable>
#include <stdexcept>

namespace PickCore {
    SessionRunState CooperativeSessionRunner::stateLocked(const SessionId id) const {
        const auto it = sessionStates_.find(id);
        if (it == sessionStates_.end()) {
            return SessionRunState::Runnable;
        }
        return it->second;
    }

    void CooperativeSessionRunner::acquire(const SessionId id) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&] {
            if (active_.has_value() && *active_ == id) {
                return true;
            }
            return stateLocked(id) != SessionRunState::WaitingForInput && !active_.has_value();
        });

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

    void CooperativeSessionRunner::runExclusive(const SessionId id, const std::function<void()> &fn) {
        acquire(id);
        fn();
        const std::lock_guard lock(mutex_);
        if (active_.has_value() && *active_ == id && depth_ > 0) {
            --depth_;
            if (depth_ == 0) {
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
} // namespace PickCore
