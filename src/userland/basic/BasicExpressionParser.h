#ifndef PICK_SYSTEM_BASIC_BASICEXPRESSIONPARSER_H
#define PICK_SYSTEM_BASIC_BASICEXPRESSIONPARSER_H

#pragma once

#include "BasicAst.h"

#include <memory>
#include <string>
#include <string_view>

namespace PickShell {
    struct BasicExpressionParseResult {
        bool success{false};
        std::unique_ptr<BasicAst::Expr> expression;
        std::string error;
    };

    class BasicExpressionParser {
    public:
        [[nodiscard]] static BasicExpressionParseResult parse(std::string_view expressionText);
    };
} // namespace PickShell

#endif // PICK_SYSTEM_BASIC_BASICEXPRESSIONPARSER_H
