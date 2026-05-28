#ifndef PICK_SYSTEM_CORE_ENGLISH_I_EXPRESSION_PARSER_H
#define PICK_SYSTEM_CORE_ENGLISH_I_EXPRESSION_PARSER_H

#include "IExpressionAst.h"

#include <memory>
#include <string>

namespace PickCore::English {
    class IExpressionParser {
    public:
        /// Parse an I-type expression string into an AST.
        /// @return Root expression or stable error in `error`.
        [[nodiscard]] static std::unique_ptr<IExpr> parse(const std::string &source, std::string &error);
    };
} // namespace PickCore::English

#endif
