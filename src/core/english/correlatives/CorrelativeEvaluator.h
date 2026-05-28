#ifndef PICK_SYSTEM_CORE_ENGLISH_CORRELATIVE_EVALUATOR_H
#define PICK_SYSTEM_CORE_ENGLISH_CORRELATIVE_EVALUATOR_H

#include "FCorrelativeDef.h"

#include "StructuredRecord.h"

#include <optional>
#include <string>

namespace PickCore::English {
    class CorrelativeEvaluator {
    public:
        /// Evaluate an F-type definition against a data record.
        /// @return Cell value, or std::nullopt with stable `error` text.
        [[nodiscard]] static std::optional<std::string> evaluateF(const FCorrelativeDef &def,
                                                                  const PickFS::StructuredRecord &dataRecord,
                                                                  std::string &error);
    };
} // namespace PickCore::English

#endif
