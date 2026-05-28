#ifndef PICK_SYSTEM_CORE_ENGLISH_ICORRELATIVE_DEF_H
#define PICK_SYSTEM_CORE_ENGLISH_ICORRELATIVE_DEF_H

#include "IExpressionAst.h"

#include <memory>
#include <string>

namespace PickCore::English {
    /// Parsed I-type DICT definition (attrs 1–2). Milestone 9 Stage 4.
    struct ICorrelativeDef {
        std::string expressionRaw;
        std::shared_ptr<const IExpr> expression;
    };
} // namespace PickCore::English

#endif
