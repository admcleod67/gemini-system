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

    struct IfStmt {
        // Required: non-null condition.
        // Guaranteed by semantic pass: thenLine/elseLine (if present) refer to existing lines.
        std::unique_ptr<BasicAst::Expr> condition;
        int thenLine{0};
        std::optional<int> elseLine;
    };

    struct PrintStmt {
        // Exactly one payload path is expected:
        // - stringLiteral has value (string print path), OR
        // - expression is non-null (numeric print path).
        std::optional<std::string> stringLiteral;
        std::unique_ptr<BasicAst::Expr> expression;
        bool suppressEol{false};
    };

    struct StopStmt {};
    struct EndStmt {};

    using NormalizedStmt = std::variant<LetStmt, InputStmt, GotoStmt, IfStmt, PrintStmt, StopStmt, EndStmt>;

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
