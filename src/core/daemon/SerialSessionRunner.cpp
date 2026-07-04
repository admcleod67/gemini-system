#include "SerialSessionRunner.h"

#include <condition_variable>

namespace PickCore {
    void SerialSessionRunner::runExclusive(const SessionId id, const std::function<void()> &fn) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&] { return !active_.has_value() || *active_ == id; });
        if (!active_.has_value()) {
            active_ = id;
            depth_ = 0;
        }
        ++depth_;
        lock.unlock();

        fn();

        lock.lock();
        --depth_;
        if (depth_ == 0) {
            active_.reset();
            cv_.notify_all();
        }
    }

    std::optional<SessionId> SerialSessionRunner::activeSession() const {
        const std::lock_guard lock(mutex_);
        return active_;
    }
} // namespace PickCore
