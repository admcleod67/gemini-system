#include <doctest/doctest.h>

#include "BasicExpressionParser.h"

#include <memory>
#include <variant>

using PickShell::BasicExpressionParser;
namespace BasicAst = PickShell::BasicAst;

namespace {
    const BasicAst::Expr *rootExpr(const PickShell::BasicExpressionParseResult &result) {
        REQUIRE(result.success);
        REQUIRE(result.expression != nullptr);
        return result.expression.get();
    }
} // namespace

TEST_CASE("basic expression parser respects arithmetic precedence and associativity") {
    const auto result = BasicExpressionParser::parse("2+3*4");
    const BasicAst::Expr *expr = rootExpr(result);
    REQUIRE(std::holds_alternative<BasicAst::BinaryExpr>(expr->node));

    const auto &add = std::get<BasicAst::BinaryExpr>(expr->node);
    CHECK(add.op == BasicAst::BinaryOp::Add);
    REQUIRE(add.left != nullptr);
    REQUIRE(add.right != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::IntLiteralExpr>(add.left->node));
    CHECK(std::get<BasicAst::IntLiteralExpr>(add.left->node).value == 2);
    REQUIRE(std::holds_alternative<BasicAst::BinaryExpr>(add.right->node));

    const auto &mul = std::get<BasicAst::BinaryExpr>(add.right->node);
    CHECK(mul.op == BasicAst::BinaryOp::Multiply);
}

TEST_CASE("basic expression parser supports nested unary minus") {
    const auto result = BasicExpressionParser::parse("--5");
    const BasicAst::Expr *expr = rootExpr(result);
    REQUIRE(std::holds_alternative<BasicAst::UnaryExpr>(expr->node));

    const auto &outer = std::get<BasicAst::UnaryExpr>(expr->node);
    REQUIRE(outer.operand != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::UnaryExpr>(outer.operand->node));
}

TEST_CASE("basic expression parser keeps comparison at lowest precedence") {
    const auto result = BasicExpressionParser::parse("1+2>=3");
    const BasicAst::Expr *expr = rootExpr(result);
    REQUIRE(std::holds_alternative<BasicAst::BinaryExpr>(expr->node));

    const auto &comparison = std::get<BasicAst::BinaryExpr>(expr->node);
    CHECK(comparison.op == BasicAst::BinaryOp::GreaterOrEqual);
    REQUIRE(comparison.left != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::BinaryExpr>(comparison.left->node));
    CHECK(std::get<BasicAst::BinaryExpr>(comparison.left->node).op == BasicAst::BinaryOp::Add);
}

TEST_CASE("basic expression parser reports malformed expression diagnostics") {
    const auto trailingOp = BasicExpressionParser::parse("2+");
    CHECK_FALSE(trailingOp.success);
    CHECK(trailingOp.error == "empty expression");

    const auto missingParen = BasicExpressionParser::parse("(1+2");
    CHECK_FALSE(missingParen.success);
    CHECK(missingParen.error == "Missing ')'");

    const auto emptyExpr = BasicExpressionParser::parse("   ");
    CHECK_FALSE(emptyExpr.success);
    CHECK(emptyExpr.error == "empty expression");
}
