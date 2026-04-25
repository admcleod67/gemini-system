#ifndef PICK_SYSTEM_BASIC_BASICNORMALIZEDIR_H
#define PICK_SYSTEM_BASIC_BASICNORMALIZEDIR_H

#pragma once

#include "BasicAst.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace PickShell::BasicIr {
    // Normalized IR contracts:
    // - Produced by BasicSemanticAnalyzer and consumed by BasicBytecodeEmitter.
    // - Control-flow targets (GOTO/IF then/else lines) are validated by semantic analysis.
    // - Emitter still guards malformed IR payloads and reports deterministic diagnostics.

    struct LetStmt {
        // Required: non-empty variableName and non-null expression.
        std::string variableName;
        std::unique_ptr<BasicAst::Expr> expression;
    };

    struct InputStmt {
        // Required: non-empty variableName.
        std::string variableName;
    };

    struct GotoStmt {
        // Guaranteed by semantic pass: targetLine refers to an existing line.
        int targetLine{0};
    };

    struct GosubStmt {
        // Guaranteed by semantic pass: targetLine refers to an existing line.
        int targetLine{0};
    };

    struct ReturnStmt {};

    struct IfStmt {
        // Required: non-null condition.
        // Guaranteed by semantic pass: thenLine/elseLine (if present) refer to existing lines.
        std::unique_ptr<BasicAst::Expr> condition;
        int thenLine{0};
        std::optional<int> elseLine;
    };

    struct PrintStmt {
        // expression is always non-null; type (string/int) inferred from expression AST at emit time.
        std::unique_ptr<BasicAst::Expr> expression;
        bool suppressEol{false};
    };

    struct RemStmt {};
    struct StopStmt {};
    struct EndStmt {};

    struct ForStmt {
        // $ variable is rejected at emit time; % variable gets CoerceInt applied to init.
        std::string variableName;
        std::unique_ptr<BasicAst::Expr> initExpr;
        std::unique_ptr<BasicAst::Expr> limitExpr;
        std::unique_ptr<BasicAst::Expr> stepExpr; // nullptr → emitter uses PUSH_INT 1
    };

    struct NextStmt {
        std::string variableName;
    };

    using NormalizedStmt = std::variant<LetStmt, InputStmt, GotoStmt, GosubStmt, ReturnStmt, ForStmt, NextStmt, IfStmt, PrintStmt, RemStmt, StopStmt, EndStmt>;

    struct NormalizedLine {
        // Original BASIC line number used for diagnostics and jump mapping.
        int lineNumber{0};
        NormalizedStmt statement;
    };

    struct NormalizedProgram {
        std::vector<NormalizedLine> lines;
    };
} // namespace PickShell::BasicIr

#endif // PICK_SYSTEM_BASIC_BASICNORMALIZEDIR_H
