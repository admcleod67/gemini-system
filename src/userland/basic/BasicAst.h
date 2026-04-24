#ifndef PICK_SYSTEM_BASIC_BASICAST_H
#define PICK_SYSTEM_BASIC_BASICAST_H

#pragma once

#include <cstddef>
#include <memory>
#include <string>
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
} // namespace PickShell::BasicAst

#endif // PICK_SYSTEM_BASIC_BASICAST_H
