#ifndef PICK_SYSTEM_CORE_ENGLISH_PLANNER_H
#define PICK_SYSTEM_CORE_ENGLISH_PLANNER_H

#include "EnglishTypes.h"

namespace PickCore::English {
    class Planner {
    public:
        [[nodiscard]] Plan makePlan(const Query &query) const { return Plan{query}; }
    };
} // namespace PickCore::English

#endif
