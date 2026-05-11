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

TEST_CASE("basic expression parser parses float literals including scientific notation") {
    const auto decimal = BasicExpressionParser::parse("3.14");
    REQUIRE(decimal.success);
    REQUIRE(decimal.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FloatLiteralExpr>(decimal.expression->node));
    CHECK(std::get<BasicAst::FloatLiteralExpr>(decimal.expression->node).value == doctest::Approx(3.14));

    const auto sci = BasicExpressionParser::parse("1E3");
    REQUIRE(sci.success);
    REQUIRE(sci.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FloatLiteralExpr>(sci.expression->node));
    CHECK(std::get<BasicAst::FloatLiteralExpr>(sci.expression->node).value == doctest::Approx(1000.0));
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
    REQUIRE(call.arguments.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::IntLiteralExpr>(call.arguments[0]->node));
    CHECK(std::get<BasicAst::IntLiteralExpr>(call.arguments[0]->node).value == 5);
}

TEST_CASE("basic expression parser parses SGN function call with negative argument") {
    const auto result = BasicExpressionParser::parse("SGN(-3)");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    const auto &call = std::get<BasicAst::FunctionCallExpr>(result.expression->node);
    CHECK(call.name == "SGN");
    REQUIRE(call.arguments.size() == 1);
}

TEST_CASE("basic expression parser parses SEQ function call with string argument") {
    const auto result = BasicExpressionParser::parse("SEQ(\"A\")");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    const auto &call = std::get<BasicAst::FunctionCallExpr>(result.expression->node);
    CHECK(call.name == "SEQ");
    REQUIRE(call.arguments.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::StringLiteralExpr>(call.arguments[0]->node));
    CHECK(std::get<BasicAst::StringLiteralExpr>(call.arguments[0]->node).value == "A");
}

TEST_CASE("basic expression parser is case-insensitive for function names") {
    const auto result = BasicExpressionParser::parse("abs(5)");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    CHECK(std::get<BasicAst::FunctionCallExpr>(result.expression->node).name == "ABS");
}

TEST_CASE("basic expression parser rejects ABS with too many arguments") {
    const auto result = BasicExpressionParser::parse("ABS(1,2)");
    CHECK_FALSE(result.success);
    REQUIRE(result.error.find("expects 1 to 1 argument") != std::string::npos);
}

TEST_CASE("basic expression parser rejects ABS with no arguments") {
    const auto result = BasicExpressionParser::parse("ABS()");
    CHECK_FALSE(result.success);
    REQUIRE(result.error.find("requires an argument") != std::string::npos);
}

TEST_CASE("basic expression parser rejects trailing comma in builtin argument list") {
    const auto result = BasicExpressionParser::parse("ABS(1,)");
    CHECK_FALSE(result.success);
    REQUIRE(result.error.find("Trailing comma") != std::string::npos);
}

TEST_CASE("basic expression parser parses LEN builtin call") {
    const auto result = BasicExpressionParser::parse("LEN(\"ABC\")");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    const auto &call = std::get<BasicAst::FunctionCallExpr>(result.expression->node);
    CHECK(call.name == "LEN");
    REQUIRE(call.arguments.size() == 1);
}

TEST_CASE("basic expression parser parses SPACE builtin call") {
    const auto result = BasicExpressionParser::parse("SPACE(3)");
    REQUIRE(result.success);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    CHECK(std::get<BasicAst::FunctionCallExpr>(result.expression->node).name == "SPACE");
}

TEST_CASE("basic expression parser parses MOD with two arguments") {
    const auto result = BasicExpressionParser::parse("MOD(7,3)");
    REQUIRE(result.success);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    const auto &call = std::get<BasicAst::FunctionCallExpr>(result.expression->node);
    CHECK(call.name == "MOD");
    REQUIRE(call.arguments.size() == 2);
}

TEST_CASE("basic expression parser rejects MOD with one argument") {
    const auto result = BasicExpressionParser::parse("MOD(5)");
    CHECK_FALSE(result.success);
    REQUIRE(result.error.find("expects 2 to 2 argument") != std::string::npos);
}

TEST_CASE("basic expression parser parses DATE and TIME with no arguments") {
    const auto d = BasicExpressionParser::parse("DATE()");
    REQUIRE(d.success);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(d.expression->node));
    CHECK(std::get<BasicAst::FunctionCallExpr>(d.expression->node).arguments.empty());

    const auto t = BasicExpressionParser::parse("TIME()");
    REQUIRE(t.success);
    CHECK(std::get<BasicAst::FunctionCallExpr>(t.expression->node).arguments.empty());
}

TEST_CASE("basic expression parser parses RND with zero or one argument") {
    const auto z = BasicExpressionParser::parse("RND()");
    REQUIRE(z.success);
    CHECK(std::get<BasicAst::FunctionCallExpr>(z.expression->node).arguments.empty());

    const auto o = BasicExpressionParser::parse("RND(42)");
    REQUIRE(o.success);
    CHECK(std::get<BasicAst::FunctionCallExpr>(o.expression->node).arguments.size() == 1);
}

TEST_CASE("basic expression parser parses nested builtin calls") {
    const auto result = BasicExpressionParser::parse("ABS(SGN(-1))");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(result.expression->node));
    const auto &outer = std::get<BasicAst::FunctionCallExpr>(result.expression->node);
    CHECK(outer.name == "ABS");
    REQUIRE(outer.arguments.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::FunctionCallExpr>(outer.arguments[0]->node));
    const auto &inner = std::get<BasicAst::FunctionCallExpr>(outer.arguments[0]->node);
    CHECK(inner.name == "SGN");
    REQUIRE(inner.arguments.size() == 1);
}

TEST_CASE("basic expression parser treats unknown name followed by paren as array subscript") {
    const auto result = BasicExpressionParser::parse("FOO(1)");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    CHECK(std::holds_alternative<BasicAst::SubscriptExpr>(result.expression->node));
}

TEST_CASE("basic expression parser accepts session @ system variable identifiers") {
    const auto lower = BasicExpressionParser::parse("@account = \"TST\"");
    REQUIRE(lower.success);
    REQUIRE(lower.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::BinaryExpr>(lower.expression->node));
    const auto &bin = std::get<BasicAst::BinaryExpr>(lower.expression->node);
    REQUIRE(std::holds_alternative<BasicAst::IdentifierExpr>(bin.left->node));
    CHECK(std::get<BasicAst::IdentifierExpr>(bin.left->node).name == "@account");

    const auto userno = BasicExpressionParser::parse("@USERNO");
    REQUIRE(userno.success);
    REQUIRE(std::holds_alternative<BasicAst::IdentifierExpr>(userno.expression->node));
    CHECK(std::get<BasicAst::IdentifierExpr>(userno.expression->node).name == "@USERNO");
}

TEST_CASE("basic expression parser rejects unknown @ identifier") {
    const auto bad = BasicExpressionParser::parse("@OTHER = 1");
    CHECK_FALSE(bad.success);
    CHECK(bad.error.find("Unknown system variable") != std::string::npos);
}

TEST_CASE("basic expression parser parses angle-bracket attribute access") {
    const auto result = BasicExpressionParser::parse("REC<2,1>");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::AttributeAccessExpr>(result.expression->node));
    const auto &expr = std::get<BasicAst::AttributeAccessExpr>(result.expression->node);
    CHECK(expr.varName == "REC");
    REQUIRE(expr.attrExpr != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::IntLiteralExpr>(expr.attrExpr->node));
    CHECK(std::get<BasicAst::IntLiteralExpr>(expr.attrExpr->node).value == 2);
    REQUIRE(expr.valueIndexExpr != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::IntLiteralExpr>(expr.valueIndexExpr->node));
    CHECK(std::get<BasicAst::IntLiteralExpr>(expr.valueIndexExpr->node).value == 1);
}

TEST_CASE("basic expression parser treats spaced less-than as comparison not attribute access") {
    const auto result = BasicExpressionParser::parse("REC < 2");
    REQUIRE(result.success);
    REQUIRE(result.expression != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::BinaryExpr>(result.expression->node));
    const auto &expr = std::get<BasicAst::BinaryExpr>(result.expression->node);
    CHECK(expr.op == BasicAst::BinaryOp::LessThan);
}

TEST_CASE("basic expression parser reports malformed angle-bracket attribute access") {
    const auto missingAttr = BasicExpressionParser::parse("REC<>");
    CHECK_FALSE(missingAttr.success);
    CHECK(missingAttr.error == "Attribute index cannot be empty");

    const auto missingClose = BasicExpressionParser::parse("REC<2,1");
    CHECK_FALSE(missingClose.success);
    CHECK(missingClose.error == "Missing '>' in attribute access");
}
