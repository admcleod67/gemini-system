#ifndef PICK_SYSTEM_CORE_LOCKING_LOCK_TABLE_H
#define PICK_SYSTEM_CORE_LOCKING_LOCK_TABLE_H

#include "LockTypes.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace PickCore::Locking {
    class LockTable {
    public:
        [[nodiscard]] bool tryAcquire(const std::string &sessionId,
                                      const std::string &fileName,
                                      const std::string &recordId,
                                      LockType lockType,
                                      LockConflict *conflictOut = nullptr);

        [[nodiscard]] bool release(const std::string &sessionId,
                                   const std::string &fileName,
                                   const std::string &recordId);

        [[nodiscard]] std::size_t releaseAllForSession(const std::string &sessionId);

        [[nodiscard]] std::size_t releaseAllForFile(const std::string &fileName);

        [[nodiscard]] std::optional<LockEntry> lookup(const std::string &fileName,
                                                      const std::string &recordId) const;

        void clearAll();

    private:
        std::unordered_map<std::string, std::unordered_map<std::string, LockEntry>> locks_;
    };
} // namespace PickCore::Locking

#endif
