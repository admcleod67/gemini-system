#ifndef PICK_SYSTEM_BASIC_BASICAST_H
#define PICK_SYSTEM_BASIC_BASICAST_H

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <variant>

namespace PickShell::BasicAst {
    struct InlineStatement;
    struct SourceRange {
        std::size_t begin{0};
        std::size_t end{0};
    };

    enum class UnaryOp {
        Negate,
    };

    enum class BinaryOp {
        Add,
        Subtract,
        Multiply,
        Divide,
        Equal,
        NotEqual,
        LessThan,
        LessOrEqual,
        GreaterThan,
        GreaterOrEqual,
    };

    struct Expr;

    struct IntLiteralExpr {
        int value{0};
        SourceRange range{};
    };

    struct FloatLiteralExpr {
        double value{0.0};
        SourceRange range{};
    };

    struct StringLiteralExpr {
        std::string value;
        SourceRange range{};
    };

    struct IdentifierExpr {
        std::string name;
        SourceRange range{};
    };

    struct UnaryExpr {
        UnaryOp op{UnaryOp::Negate};
        std::unique_ptr<Expr> operand;
        SourceRange range{};
    };

    struct BinaryExpr {
        BinaryOp op{BinaryOp::Add};
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
        SourceRange range{};
    };

    struct GroupedExpr {
        std::unique_ptr<Expr> expression;
        SourceRange range{};
    };

    struct SubscriptExpr {
        std::string varName;
        std::unique_ptr<Expr> indexExpr;
        SourceRange range{};
    };

    struct FunctionCallExpr {
        std::string name;  // uppercased: "ABS", "SGN", "SEQ"
        std::unique_ptr<Expr> argument;
        SourceRange range{};
    };

    using ExprNode = std::variant<IntLiteralExpr, FloatLiteralExpr, StringLiteralExpr, IdentifierExpr, UnaryExpr, BinaryExpr, GroupedExpr, SubscriptExpr, FunctionCallExpr>;

    struct Expr {
        ExprNode node;
        SourceRange range{};
    };

    struct LetStmt {
        std::string variableName;
        std::unique_ptr<Expr> expression;
    };

    struct InputStmt {
        std::string variableName;
    };

    struct GotoStmt {
        int targetLine{0};
    };

    struct GosubStmt {
        int targetLine{0};
    };

    struct ReturnStmt {};

    struct ForStmt {
        std::string variableName;
        std::unique_ptr<Expr> initExpr;
        std::unique_ptr<Expr> limitExpr;
        std::unique_ptr<Expr> stepExpr; // nullptr → step defaults to 1 at emit time
    };

    struct NextStmt {
        std::string variableName;
    };

    struct BranchArm {
        std::optional<int> line;
        std::shared_ptr<InlineStatement> inlineStatement;
    };

    struct IfStmt {
        std::unique_ptr<Expr> condition;
        BranchArm thenArm;
        std::optional<BranchArm> elseArm;
    };

    struct PrintStmt {
        std::unique_ptr<Expr> expression;
        bool suppressEol{false};
    };

    struct RemStmt {};

    struct StopStmt {};

    struct EndStmt {};

    struct DimStmt {
        std::string variableName;
        std::unique_ptr<Expr> sizeExpr;
    };

    struct LetArrayStmt {
        std::string variableName;
        std::unique_ptr<Expr> indexExpr;
        std::unique_ptr<Expr> valueExpr;
    };

    struct ClearStmt {};

    struct OpenStmt {
        std::unique_ptr<Expr> fileExpr;
        std::string fileVar;
        std::optional<BranchArm> elseArm;
    };

    struct ReadStmt {
        std::string targetVar;
        std::string fileVar;
        std::unique_ptr<Expr> idExpr;
        std::optional<BranchArm> elseArm;
    };

    struct WriteStmt {
        std::unique_ptr<Expr> valueExpr;
        std::string fileVar;
        std::unique_ptr<Expr> idExpr;
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
        std::unique_ptr<Expr> idExpr;
        std::unique_ptr<Expr> attrExpr;
        std::unique_ptr<Expr> valueIndexExpr; // nullptr means attribute-level read
        std::optional<BranchArm> elseArm;
    };

    struct WriteVStmt {
        std::unique_ptr<Expr> valueExpr;
        std::string fileVar;
        std::unique_ptr<Expr> idExpr;
        std::unique_ptr<Expr> attrExpr;
        std::unique_ptr<Expr> valueIndexExpr; // nullptr means attribute-level write
        std::optional<BranchArm> elseArm;
    };

    struct CloseStmt {
        std::string fileVar;
    };

    using StatementNode = std::variant<LetStmt, InputStmt, GotoStmt, GosubStmt, ReturnStmt, ForStmt, NextStmt, IfStmt, PrintStmt, RemStmt, StopStmt, EndStmt, DimStmt, LetArrayStmt, ClearStmt, OpenStmt, ReadStmt, WriteStmt, ReadNextStmt, ReadVStmt, WriteVStmt, CloseStmt>;

    struct InlineStatement {
        StatementNode statement;
    };

    struct ParsedBasicLine {
        int lineNumber{0};
        StatementNode statement;
    };

    struct StatementParseError {
        int line{0};
        std::string message;
    };

    struct StatementParseResult {
        bool success{false};
        std::vector<ParsedBasicLine> lines;
        std::vector<StatementParseError> errors;
    };

    struct SemanticError {
        int line{0};
        std::string message;
    };

    struct SemanticLine {
        int lineNumber{0};
        StatementNode statement;
    };

    struct SemanticResult {
        bool success{false};
        std::vector<SemanticLine> lines;
        std::vector<SemanticError> errors;
    };
} // namespace PickShell::BasicAst

#endif // PICK_SYSTEM_BASIC_BASICAST_H
