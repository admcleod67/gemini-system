#ifndef PICK_SYSTEM_BASIC_BASICNORMALIZEDIR_H
#define PICK_SYSTEM_BASIC_BASICNORMALIZEDIR_H

#pragma once

#include "BasicAst.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace PickShell::BasicIr {
    struct InlineStatement;
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
        // Optional prompt expression for INPUT "Prompt", var lowering.
        std::unique_ptr<BasicAst::Expr> promptExpr;
        // Required: non-empty variableName.
        std::string variableName;
    };

    struct ChainStmt {
        // Required: program expression evaluating to program name.
        std::unique_ptr<BasicAst::Expr> programExpr;
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

    struct BranchArm {
        std::optional<int> line;
        std::shared_ptr<InlineStatement> inlineStatement;
    };

    struct IfStmt {
        // Required: non-null condition.
        // Guaranteed by semantic pass: thenArm.line / elseArm.line (when present) refer to existing lines.
        std::unique_ptr<BasicAst::Expr> condition;
        BranchArm thenArm;
        std::optional<BranchArm> elseArm;
    };

    struct PrintStmt {
        // expression is always non-null; type (string/int) inferred from expression AST at emit time.
        std::unique_ptr<BasicAst::Expr> expression;
        bool suppressEol{false};
    };

    struct RemStmt {};
    struct StopStmt {};
    struct EndStmt {};

    struct ForStmt {        // $ variable is rejected at emit time; % variable gets CoerceInt applied to init.
        std::string variableName;
        std::unique_ptr<BasicAst::Expr> initExpr;
        std::unique_ptr<BasicAst::Expr> limitExpr;
        std::unique_ptr<BasicAst::Expr> stepExpr; // nullptr → emitter uses PUSH_INT 1
    };

    struct NextStmt {
        std::string variableName;
    };

    struct DimStmt {
        std::string variableName;
        std::unique_ptr<BasicAst::Expr> sizeExpr;
    };

    struct LetArrayStmt {
        std::string variableName;
        std::unique_ptr<BasicAst::Expr> indexExpr;
        std::unique_ptr<BasicAst::Expr> valueExpr;
    };

    struct ClearStmt {};

    struct OpenStmt {
        std::unique_ptr<BasicAst::Expr> fileExpr;
        std::string fileVar;
        std::optional<BranchArm> elseArm;
    };

    struct ReadStmt {
        std::string targetVar;
        std::string fileVar;
        std::unique_ptr<BasicAst::Expr> idExpr;
        std::optional<BranchArm> elseArm;
    };

    struct ReadUStmt {
        std::string targetVar;
        std::string fileVar;
        std::unique_ptr<BasicAst::Expr> idExpr;
        std::optional<BranchArm> elseArm;
    };

    struct WriteStmt {
        std::unique_ptr<BasicAst::Expr> valueExpr;
        std::string fileVar;
        std::unique_ptr<BasicAst::Expr> idExpr;
        std::optional<BranchArm> elseArm;
    };

    struct WriteUStmt {
        std::unique_ptr<BasicAst::Expr> valueExpr;
        std::string fileVar;
        std::unique_ptr<BasicAst::Expr> idExpr;
        std::optional<BranchArm> elseArm;
    };

    struct ReadNextStmt {
        std::string targetVar;
        std::string fileVar;
        std::optional<BranchArm> elseArm;
    };

    struct ReadVStmt {
        std::string targetVar;
        std::string fileVar;
        std::unique_ptr<BasicAst::Expr> idExpr;
        std::unique_ptr<BasicAst::Expr> attrExpr;
        std::unique_ptr<BasicAst::Expr> valueIndexExpr; // nullptr means attribute-level read
        std::optional<BranchArm> elseArm;
    };

    struct WriteVStmt {
        std::unique_ptr<BasicAst::Expr> valueExpr;
        std::string fileVar;
        std::unique_ptr<BasicAst::Expr> idExpr;
        std::unique_ptr<BasicAst::Expr> attrExpr;
        std::unique_ptr<BasicAst::Expr> valueIndexExpr; // nullptr means attribute-level write
        std::optional<BranchArm> elseArm;
    };

    struct CloseStmt {
        std::string fileVar;
    };

    struct ReleaseStmt {
        std::string fileVar;
        std::unique_ptr<BasicAst::Expr> idExpr;
    };

    struct OnErrorStmt {
        bool stop{false};
        int targetLine{0};
    };

    // MAT statements (Milestone 7 Stage 7).
    // - MatAssignStmt: either rhsExpr (scalar broadcast) or rhsSourceArray (MAT copy) is populated.
    // - MAT READ / MAT WRITE mirror ReadStmt / WriteStmt and reuse the same ELSE arm semantics.
    struct MatAssignStmt {
        std::string targetArray;
        std::unique_ptr<BasicAst::Expr> rhsExpr;
        std::string rhsSourceArray;
    };

    struct MatReadStmt {
        std::string targetArray;
        std::string fileVar;
        std::unique_ptr<BasicAst::Expr> idExpr;
        std::optional<BranchArm> elseArm;
    };

    struct MatWriteStmt {
        std::string sourceArray;
        std::string fileVar;
        std::unique_ptr<BasicAst::Expr> idExpr;
        std::optional<BranchArm> elseArm;
    };

    using NormalizedStmt = std::variant<LetStmt, InputStmt, ChainStmt, GotoStmt, GosubStmt, ReturnStmt, ForStmt, NextStmt, IfStmt, PrintStmt, RemStmt, StopStmt, EndStmt, DimStmt, LetArrayStmt, ClearStmt, OpenStmt, ReadStmt, ReadUStmt, WriteStmt, WriteUStmt, ReadNextStmt, ReadVStmt, WriteVStmt, CloseStmt, ReleaseStmt, OnErrorStmt, MatAssignStmt, MatReadStmt, MatWriteStmt>;

    struct InlineStatement {
        NormalizedStmt statement;
    };

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
