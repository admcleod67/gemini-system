//
// Pick port allocator for daemon session objects (Milestone 13 Stage 3).
//

#ifndef PICK_SYSTEM_CORE_DAEMON_PORT_MANAGER_H
#define PICK_SYSTEM_CORE_DAEMON_PORT_MANAGER_H

#include "SerialSessionRunner.h"

#include <cstddef>
#include <iosfwd>
#include <mutex>
#include <optional>
#include <set>

namespace PickCore {
    class PortManager {
    public:
        explicit PortManager(std::size_t maxPorts = 64);

        [[nodiscard]] std::optional<SessionId> allocate();
        void release(SessionId port);

        [[nodiscard]] std::size_t inUseCount() const;
        [[nodiscard]] std::size_t maxPorts() const;

        void formatBootStatus(std::ostream &out) const;

    private:
        std::size_t maxPorts_;
        mutable std::mutex mutex_;
        std::set<SessionId> inUse_;
        SessionId nextCandidate_{1};
    };
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_PORT_MANAGER_H
