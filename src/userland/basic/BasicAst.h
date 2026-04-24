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

    using ExprNode = std::variant<IntLiteralExpr, IdentifierExpr, UnaryExpr, BinaryExpr, GroupedExpr>;

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

    struct IfStmt {
        std::unique_ptr<Expr> condition;
        int thenLine{0};
        std::optional<int> elseLine;
    };

    struct PrintStmt {
        std::optional<std::string> stringLiteral;
        std::unique_ptr<Expr> expression;
        bool suppressEol{false};
    };

    struct StopStmt {};

    struct EndStmt {};

    using StatementNode = std::variant<LetStmt, InputStmt, GotoStmt, IfStmt, PrintStmt, StopStmt, EndStmt>;

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
