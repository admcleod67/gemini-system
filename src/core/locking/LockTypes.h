#ifndef PICK_SYSTEM_CORE_LOCKING_LOCK_TYPES_H
#define PICK_SYSTEM_CORE_LOCKING_LOCK_TYPES_H

#include <chrono>
#include <string>

namespace PickCore::Locking {
    enum class LockType {
        ReadU,
        WriteU,
    };

    struct LockEntry {
        std::string ownerSessionId;
        LockType lockType{LockType::ReadU};
        std::chrono::steady_clock::time_point acquiredAt{};
    };

    struct LockConflict {
        std::string fileName;
        std::string recordId;
        std::string ownerSessionId;
        LockType lockType{LockType::ReadU};
    };

    [[nodiscard]] std::string lockTypeLabel(LockType type);

    [[nodiscard]] std::string describeLockConflict(const LockConflict &conflict);
} // namespace PickCore::Locking

#endif
