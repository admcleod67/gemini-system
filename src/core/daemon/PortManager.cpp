#include "PortManager.h"

#include <ostream>

namespace PickCore {
    PortManager::PortManager(const std::size_t maxPorts) : maxPorts_(maxPorts < 1 ? 1 : maxPorts) {}

    std::optional<SessionId> PortManager::allocate() {
        std::lock_guard lock(mutex_);
        if (inUse_.size() >= maxPorts_) {
            return std::nullopt;
        }

        SessionId candidate = nextCandidate_;
        while (inUse_.count(candidate) != 0) {
            ++candidate;
        }
        inUse_.insert(candidate);
        nextCandidate_ = candidate + 1;
        return candidate;
    }

    void PortManager::release(const SessionId port) {
        std::lock_guard lock(mutex_);
        inUse_.erase(port);
        if (port < nextCandidate_) {
            nextCandidate_ = port;
        }
    }

    std::size_t PortManager::inUseCount() const {
        std::lock_guard lock(mutex_);
        return inUse_.size();
    }

    std::size_t PortManager::maxPorts() const {
        return maxPorts_;
    }

    void PortManager::formatBootStatus(std::ostream &out) const {
        out << "PORT MANAGER: READY\n";
    }
} // namespace PickCore
