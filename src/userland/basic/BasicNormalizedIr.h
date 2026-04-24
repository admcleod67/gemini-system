#ifndef PICK_SYSTEM_BASIC_BASICNORMALIZEDIR_H
#define PICK_SYSTEM_BASIC_BASICNORMALIZEDIR_H

#pragma once

#include "BasicAst.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace PickShell::BasicIr {
    struct LetStmt {
        std::string variableName;
        std::unique_ptr<BasicAst::Expr> expression;
    };

    struct InputStmt {
        std::string variableName;
    };

    struct GotoStmt {
        int targetLine{0};
    };

    struct IfStmt {
        std::unique_ptr<BasicAst::Expr> condition;
        int thenLine{0};
        std::optional<int> elseLine;
    };

    struct PrintStmt {
        std::optional<std::string> stringLiteral;
        std::unique_ptr<BasicAst::Expr> expression;
        bool suppressEol{false};
    };

    struct StopStmt {};
    struct EndStmt {};

    using NormalizedStmt = std::variant<LetStmt, InputStmt, GotoStmt, IfStmt, PrintStmt, StopStmt, EndStmt>;

    struct NormalizedLine {
        int lineNumber{0};
        NormalizedStmt statement;
    };

    struct NormalizedProgram {
        std::vector<NormalizedLine> lines;
    };
} // namespace PickShell::BasicIr

#endif // PICK_SYSTEM_BASIC_BASICNORMALIZEDIR_H
