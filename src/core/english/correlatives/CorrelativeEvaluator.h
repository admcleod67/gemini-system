#ifndef PICK_SYSTEM_CORE_ENGLISH_CORRELATIVE_EVALUATOR_H
#define PICK_SYSTEM_CORE_ENGLISH_CORRELATIVE_EVALUATOR_H

#include "EnglishTypes.h"
#include "FCorrelativeDef.h"
#include "ICorrelativeDef.h"

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

        [[nodiscard]] static std::optional<std::string> evaluateI(const ICorrelativeDef &def,
                                                                  const PickFS::StructuredRecord &dataRecord,
                                                                  std::string &error);

        /// Resolve one DICT field to a display/sort cell (Milestone 9 Stage 2).
        [[nodiscard]] static std::string evaluateFieldCell(const FieldRef &ref,
                                                           const PickFS::StructuredRecord &dataRecord);
    };
} // namespace PickCore::English

#endif
