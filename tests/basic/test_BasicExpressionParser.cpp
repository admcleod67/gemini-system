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

TEST_CASE("basic expression parser parses string literal") {
    const auto result = BasicExpressionParser::parse("\"hello world\"");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::StringLiteralExpr>(result.expression->node));
    CHECK(std::get<BasicAst::StringLiteralExpr>(result.expression->node).value == "hello world");
}

TEST_CASE("basic expression parser parses string variable identifier") {
    const auto result = BasicExpressionParser::parse("A$");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::IdentifierExpr>(result.expression->node));
    CHECK(std::get<BasicAst::IdentifierExpr>(result.expression->node).name == "A$");
}

TEST_CASE("basic expression parser reports unterminated string literal") {
    const auto result = BasicExpressionParser::parse("\"hello");
    CHECK_FALSE(result.success);
    CHECK(result.error == "Unterminated string literal");
}

TEST_CASE("basic expression parser parses string equality comparison") {
    const auto result = BasicExpressionParser::parse("A$ = \"hello\"");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::BinaryExpr>(result.expression->node));
    const auto &bin = std::get<BasicAst::BinaryExpr>(result.expression->node);
    CHECK(bin.op == BasicAst::BinaryOp::Equal);
    REQUIRE(std::holds_alternative<BasicAst::IdentifierExpr>(bin.left->node));
    CHECK(std::get<BasicAst::IdentifierExpr>(bin.left->node).name == "A$");
    REQUIRE(std::holds_alternative<BasicAst::StringLiteralExpr>(bin.right->node));
    CHECK(std::get<BasicAst::StringLiteralExpr>(bin.right->node).value == "hello");
}

TEST_CASE("basic expression parser parses percent variable identifier") {
    const auto result = BasicExpressionParser::parse("COUNT%");
    REQUIRE(result.success);
    REQUIRE(result.expression);
    REQUIRE(std::holds_alternative<BasicAst::IdentifierExpr>(result.expression->node));
    CHECK(std::get<BasicAst::IdentifierExpr>(result.expression->node).name == "COUNT%");
}

TEST_CASE("basic expression parser parses percent variable in arithmetic") {
    const auto result = BasicExpressionParser::parse("A% + 1");
    REQUIRE(result.success);
    REQUIRE(result.expression);
    const auto &bin = std::get<BasicAst::BinaryExpr>(result.expression->node);
    CHECK(bin.op == BasicAst::BinaryOp::Add);
    REQUIRE(std::holds_alternative<BasicAst::IdentifierExpr>(bin.left->node));
    CHECK(std::get<BasicAst::IdentifierExpr>(bin.left->node).name == "A%");
}

TEST_CASE("basic expression parser parses ABS function call") {
    const auto result = BasicExpressionParser::parse("ABS(5)");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    const auto &call = std::get<BasicAst::FunctionCallExpr>(result.expression->node);
    CHECK(call.name == "ABS");
    REQUIRE(call.argument != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::IntLiteralExpr>(call.argument->node));
    CHECK(std::get<BasicAst::IntLiteralExpr>(call.argument->node).value == 5);
}

TEST_CASE("basic expression parser parses SGN function call with negative argument") {
    const auto result = BasicExpressionParser::parse("SGN(-3)");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    const auto &call = std::get<BasicAst::FunctionCallExpr>(result.expression->node);
    CHECK(call.name == "SGN");
    REQUIRE(call.argument != nullptr);
}

TEST_CASE("basic expression parser parses SEQ function call with string argument") {
    const auto result = BasicExpressionParser::parse("SEQ(\"A\")");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    const auto &call = std::get<BasicAst::FunctionCallExpr>(result.expression->node);
    CHECK(call.name == "SEQ");
    REQUIRE(call.argument != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::StringLiteralExpr>(call.argument->node));
    CHECK(std::get<BasicAst::StringLiteralExpr>(call.argument->node).value == "A");
}

TEST_CASE("basic expression parser is case-insensitive for function names") {
    const auto result = BasicExpressionParser::parse("abs(5)");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    CHECK(std::get<BasicAst::FunctionCallExpr>(result.expression->node).name == "ABS");
}

TEST_CASE("basic expression parser treats unknown name followed by paren as array subscript") {
    const auto result = BasicExpressionParser::parse("FOO(1)");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    CHECK(std::holds_alternative<BasicAst::SubscriptExpr>(result.expression->node));
}
