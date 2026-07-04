#ifndef PICK_SYSTEM_CORE_LOCKING_LOCK_REGISTRY_H
#define PICK_SYSTEM_CORE_LOCKING_LOCK_REGISTRY_H

#include "LockTable.h"

#include <memory>

namespace PickCore::Locking {
    /// Process-wide shared lock table (Milestone 10 Stage 2).
    /// GeminiServiceDaemon adopts its table here (M13 Stage 2).
    class LockRegistry {
    public:
        [[nodiscard]] static LockRegistry &instance() {
            static LockRegistry registry;
            return registry;
        }

        static void adoptTable(std::shared_ptr<LockTable> table);

        static void resetForTests();

        [[nodiscard]] std::shared_ptr<LockTable> table() const { return table_; }

    private:
        LockRegistry() : table_(std::make_shared<LockTable>()) {}

        std::shared_ptr<LockTable> table_;
    };
} // namespace PickCore::Locking

#endif // PICK_SYSTEM_CORE_LOCKING_LOCK_REGISTRY_H
