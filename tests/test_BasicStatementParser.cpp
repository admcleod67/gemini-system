#include <doctest/doctest.h>

#include "BasicProgram.h"
#include "BasicStatementParser.h"

#include <variant>

using PickShell::BasicProgram;
using PickShell::BasicStatementParser;

namespace BasicAst = PickShell::BasicAst;

TEST_CASE("basic statement parser parses each supported statement kind") {
    BasicProgram program;
    program.setLine(10, "LET A = 1+2");
    program.setLine(20, "INPUT X");
    program.setLine(30, "GOTO 50");
    program.setLine(40, "IF A = 1 THEN 10 ELSE 20");
    program.setLine(50, "PRINT \"OK\";");
    program.setLine(55, "REM this is a comment");
    program.setLine(60, "STOP");
    program.setLine(70, "END");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK(result.success);
    CHECK(result.errors.empty());
    REQUIRE(result.lines.size() == 8);
    CHECK(std::holds_alternative<BasicAst::LetStmt>(result.lines[0].statement));
    CHECK(std::holds_alternative<BasicAst::InputStmt>(result.lines[1].statement));
    CHECK(std::holds_alternative<BasicAst::GotoStmt>(result.lines[2].statement));
    CHECK(std::holds_alternative<BasicAst::IfStmt>(result.lines[3].statement));
    CHECK(std::holds_alternative<BasicAst::PrintStmt>(result.lines[4].statement));
    CHECK(std::holds_alternative<BasicAst::RemStmt>(result.lines[5].statement));
    CHECK(std::holds_alternative<BasicAst::StopStmt>(result.lines[6].statement));
    CHECK(std::holds_alternative<BasicAst::EndStmt>(result.lines[7].statement));
}

TEST_CASE("basic statement parser handles REM with and without comment text") {
    BasicProgram program;
    program.setLine(10, "REM");
    program.setLine(20, "REM this is a comment");
    program.setLine(30, "REM    leading spaces are fine too");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK(result.success);
    CHECK(result.errors.empty());
    REQUIRE(result.lines.size() == 3);
    CHECK(std::holds_alternative<BasicAst::RemStmt>(result.lines[0].statement));
    CHECK(std::holds_alternative<BasicAst::RemStmt>(result.lines[1].statement));
    CHECK(std::holds_alternative<BasicAst::RemStmt>(result.lines[2].statement));
}

TEST_CASE("basic statement parser preserves IF THEN ELSE token boundary behavior") {
    BasicProgram program;
    program.setLine(10, "IF A THENB 20");
    program.setLine(20, "IF A THEN 30 ELSEX 40");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 2);
    CHECK(result.errors[0].message == "IF requires THEN <line>");
    CHECK(result.errors[1].message == "IF THEN requires a line number");
}

TEST_CASE("basic statement parser preserves PRINT string and semicolon behavior") {
    BasicProgram program;
    program.setLine(10, "PRINT \"HELLO\";");
    program.setLine(20, "PRINT 1+1");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK(result.success);
    REQUIRE(result.lines.size() == 2);

    const auto &printString = std::get<BasicAst::PrintStmt>(result.lines[0].statement);
    REQUIRE(printString.expression != nullptr);
    const auto *strLit = std::get_if<BasicAst::StringLiteralExpr>(&printString.expression->node);
    REQUIRE(strLit != nullptr);
    CHECK(strLit->value == "HELLO");
    CHECK(printString.suppressEol);

    const auto &printExpr = std::get<BasicAst::PrintStmt>(result.lines[1].statement);
    REQUIRE(printExpr.expression != nullptr);
    CHECK(std::holds_alternative<BasicAst::BinaryExpr>(printExpr.expression->node));
    CHECK_FALSE(printExpr.suppressEol);
}

TEST_CASE("basic statement parser preserves diagnostic messages") {
    BasicProgram program;
    program.setLine(10, "LET A 1");
    program.setLine(20, "INPUT A B");
    program.setLine(30, "GOTO X");
    program.setLine(40, "IF 1 THEN");
    program.setLine(50, "PRINT");
    program.setLine(60, "STOP 1");
    program.setLine(70, "END 1");
    program.setLine(80, "MAT A = 1");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 8);
    CHECK(result.errors[0].message == "LET requires assignment with '='");
    CHECK(result.errors[1].message == "INPUT requires a variable name");
    CHECK(result.errors[2].message == "GOTO requires a line number");
    CHECK(result.errors[3].message == "IF THEN requires a line number");
    CHECK(result.errors[4].message == "PRINT requires an expression");
    CHECK(result.errors[5].message == "STOP takes no arguments");
    CHECK(result.errors[6].message == "END takes no arguments");
    CHECK(result.errors[7].message == "Unknown keyword 'MAT'");
}
