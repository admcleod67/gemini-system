#ifndef PICK_SYSTEM_CORE_ENGLISH_I_EXPRESSION_EVALUATOR_H
#define PICK_SYSTEM_CORE_ENGLISH_I_EXPRESSION_EVALUATOR_H

#include "IExpressionAst.h"

#include "StructuredRecord.h"

#include <optional>
#include <string>

namespace PickCore::English {
    class IExpressionEvaluator {
    public:
        [[nodiscard]] static std::optional<std::string> evaluate(const IExpr &expr,
                                                               const PickFS::StructuredRecord &dataRecord,
                                                               std::string &error);
    };
} // namespace PickCore::English

#endif
