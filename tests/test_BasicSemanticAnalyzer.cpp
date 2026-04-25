#include <doctest/doctest.h>

#include "BasicProgram.h"
#include "BasicNormalizedIr.h"
#include "BasicSemanticAnalyzer.h"
#include "BasicStatementParser.h"

#include <utility>

using PickShell::BasicProgram;
using PickShell::BasicSemanticAnalyzer;
using PickShell::BasicStatementParser;
namespace BasicIr = PickShell::BasicIr;

TEST_CASE("basic semantic analyzer passes when control flow targets exist") {
    BasicProgram program;
    program.setLine(10, "IF 1 THEN 30 ELSE 40");
    program.setLine(20, "PRINT 1");
    program.setLine(30, "GOTO 20");
    program.setLine(40, "END");

    auto parsed = BasicStatementParser::parse(program);
    REQUIRE(parsed.success);

    const auto semantic = BasicSemanticAnalyzer::analyze(std::move(parsed));
    CHECK(semantic.success);
    CHECK(semantic.errors.empty());
    CHECK(semantic.program.lines.size() == 4);
}

TEST_CASE("basic semantic analyzer reports missing GOTO target") {
    BasicProgram program;
    program.setLine(10, "GOTO 99");

    auto parsed = BasicStatementParser::parse(program);
    REQUIRE(parsed.success);

    const auto semantic = BasicSemanticAnalyzer::analyze(std::move(parsed));
    CHECK_FALSE(semantic.success);
    REQUIRE(semantic.errors.size() == 1);
    CHECK(semantic.errors[0].line == 10);
    CHECK(semantic.errors[0].message == "Unknown target line 99");
}

TEST_CASE("basic semantic analyzer reports missing IF THEN target") {
    BasicProgram program;
    program.setLine(10, "IF 1 THEN 99");

    auto parsed = BasicStatementParser::parse(program);
    REQUIRE(parsed.success);

    const auto semantic = BasicSemanticAnalyzer::analyze(std::move(parsed));
    CHECK_FALSE(semantic.success);
    REQUIRE(semantic.errors.size() == 1);
    CHECK(semantic.errors[0].message == "Unknown target line 99");
}

TEST_CASE("basic semantic analyzer reports missing IF ELSE target") {
    BasicProgram program;
    program.setLine(10, "IF 1 THEN 20 ELSE 99");
    program.setLine(20, "END");

    auto parsed = BasicStatementParser::parse(program);
    REQUIRE(parsed.success);

    const auto semantic = BasicSemanticAnalyzer::analyze(std::move(parsed));
    CHECK_FALSE(semantic.success);
    REQUIRE(semantic.errors.size() == 1);
    CHECK(semantic.errors[0].line == 10);
    CHECK(semantic.errors[0].message == "Unknown target line 99");
}

TEST_CASE("basic semantic analyzer reports multiple missing targets in stable order") {
    BasicProgram program;
    program.setLine(10, "GOTO 99");
    program.setLine(20, "IF 1 THEN 98 ELSE 97");

    auto parsed = BasicStatementParser::parse(program);
    REQUIRE(parsed.success);

    const auto semantic = BasicSemanticAnalyzer::analyze(std::move(parsed));
    CHECK_FALSE(semantic.success);
    REQUIRE(semantic.errors.size() == 3);
    CHECK(semantic.errors[0].message == "Unknown target line 99");
    CHECK(semantic.errors[1].message == "Unknown target line 98");
    CHECK(semantic.errors[2].message == "Unknown target line 97");
}

TEST_CASE("basic semantic analyzer passes when GOSUB target exists") {
    BasicProgram program;
    program.setLine(10, "GOSUB 30");
    program.setLine(20, "END");
    program.setLine(30, "RETURN");

    auto parsed = BasicStatementParser::parse(program);
    REQUIRE(parsed.success);

    const auto semantic = BasicSemanticAnalyzer::analyze(std::move(parsed));
    CHECK(semantic.success);
    CHECK(semantic.errors.empty());
    CHECK(semantic.program.lines.size() == 3);
}

TEST_CASE("basic semantic analyzer reports missing GOSUB target") {
    BasicProgram program;
    program.setLine(10, "GOSUB 99");

    auto parsed = BasicStatementParser::parse(program);
    REQUIRE(parsed.success);

    const auto semantic = BasicSemanticAnalyzer::analyze(std::move(parsed));
    CHECK_FALSE(semantic.success);
    REQUIRE(semantic.errors.size() == 1);
    CHECK(semantic.errors[0].line == 10);
    CHECK(semantic.errors[0].message == "Unknown target line 99");
}

TEST_CASE("basic semantic analyzer lowers FOR/NEXT to IR") {
    BasicProgram program;
    program.setLine(10, "FOR I = 1 TO 10");
    program.setLine(20, "NEXT I");

    auto parsed = BasicStatementParser::parse(program);
    REQUIRE(parsed.success);

    const auto semantic = BasicSemanticAnalyzer::analyze(std::move(parsed));
    REQUIRE(semantic.success);
    REQUIRE(semantic.program.lines.size() == 2);
    CHECK(std::holds_alternative<BasicIr::ForStmt>(semantic.program.lines[0].statement));
    CHECK(std::holds_alternative<BasicIr::NextStmt>(semantic.program.lines[1].statement));
    const auto &forStmt = std::get<BasicIr::ForStmt>(semantic.program.lines[0].statement);
    CHECK(forStmt.variableName == "I");
    CHECK(forStmt.initExpr != nullptr);
    CHECK(forStmt.limitExpr != nullptr);
    CHECK(forStmt.stepExpr == nullptr);
    CHECK(std::get<BasicIr::NextStmt>(semantic.program.lines[1].statement).variableName == "I");
}
