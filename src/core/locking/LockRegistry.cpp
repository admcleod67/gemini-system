#include "LockRegistry.h"

namespace PickCore::Locking {
    void LockRegistry::adoptTable(const std::shared_ptr<LockTable> table) {
        if (table == nullptr) {
            return;
        }
        instance().table_ = table;
    }

    void LockRegistry::resetForTests() {
        instance().table_->clearAll();
    }
} // namespace PickCore::Locking
