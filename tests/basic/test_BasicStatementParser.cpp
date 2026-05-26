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
    program.setLine(80, "CHAIN \"NEXT\"");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK(result.success);
    CHECK(result.errors.empty());
    REQUIRE(result.lines.size() == 9);
    CHECK(std::holds_alternative<BasicAst::LetStmt>(result.lines[0].statement));
    CHECK(std::holds_alternative<BasicAst::InputStmt>(result.lines[1].statement));
    CHECK(std::holds_alternative<BasicAst::GotoStmt>(result.lines[2].statement));
    CHECK(std::holds_alternative<BasicAst::IfStmt>(result.lines[3].statement));
    CHECK(std::holds_alternative<BasicAst::PrintStmt>(result.lines[4].statement));
    CHECK(std::holds_alternative<BasicAst::RemStmt>(result.lines[5].statement));
    CHECK(std::holds_alternative<BasicAst::StopStmt>(result.lines[6].statement));
    CHECK(std::holds_alternative<BasicAst::EndStmt>(result.lines[7].statement));
    CHECK(std::holds_alternative<BasicAst::ChainStmt>(result.lines[8].statement));
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
    CHECK(result.errors[1].message == "Unknown keyword '30'");
    CHECK(result.lines.empty());
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
    program.setLine(80, "ZAP A = 1");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 8);
    CHECK(result.errors[0].message == "LET requires assignment with '='");
    CHECK(result.errors[1].message == "INPUT requires a variable name");
    CHECK(result.errors[2].message == "GOTO requires a line number");
    CHECK(result.errors[3].message == "IF THEN requires a line number or statement");
    CHECK(result.errors[4].message == "PRINT requires an expression");
    CHECK(result.errors[5].message == "STOP takes no arguments");
    CHECK(result.errors[6].message == "END takes no arguments");
    CHECK(result.errors[7].message == "Unknown keyword 'ZAP'");
}

TEST_CASE("basic statement parser supports prompted INPUT") {
    BasicProgram program;
    program.setLine(10, "INPUT \"Enter value: \", A");
    program.setLine(20, "INPUT A$");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK(result.success);
    REQUIRE(result.lines.size() == 2);

    REQUIRE(std::holds_alternative<BasicAst::InputStmt>(result.lines[0].statement));
    const auto &prompted = std::get<BasicAst::InputStmt>(result.lines[0].statement);
    CHECK(prompted.promptExpr != nullptr);
    CHECK(prompted.variableName == "A");
    REQUIRE(std::holds_alternative<BasicAst::InputStmt>(result.lines[1].statement));
    const auto &plain = std::get<BasicAst::InputStmt>(result.lines[1].statement);
    CHECK(plain.promptExpr == nullptr);
    CHECK(plain.variableName == "A$");
}

TEST_CASE("basic statement parser reports malformed prompted INPUT") {
    BasicProgram program;
    program.setLine(10, "INPUT , A");
    program.setLine(20, "INPUT \"X\", ");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 2);
    CHECK(result.errors[0].message == "INPUT prompt expression cannot be empty");
    CHECK(result.errors[1].message == "INPUT requires a variable name");
}

TEST_CASE("basic statement parser keeps precise malformed file-statement diagnostics") {
    BasicProgram program;
    program.setLine(10, "OPEN");
    program.setLine(20, "READ REC F, ID");
    program.setLine(30, "WRITE ON F, ID");
    program.setLine(40, "READNEXT FROM F");
    program.setLine(50, "READV X FROM F,");
    program.setLine(60, "WRITEV \"A\" F, ID, 1");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 6);
    CHECK(result.errors[0].message == "OPEN requires a file expression and TO <filevar>");
    CHECK(result.errors[1].message == "READ requires FROM");
    CHECK(result.errors[2].message == "WRITE requires a value expression");
    CHECK(result.errors[3].message == "READNEXT requires a valid target variable name");
    CHECK(result.errors[4].message == "READV requires <filevar>, <id>, <attr>[, <value-index>]");
    CHECK(result.errors[5].message == "WRITEV requires ON");
}

TEST_CASE("basic statement parser parses GOSUB") {
    BasicProgram program;
    program.setLine(10, "GOSUB 100");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::GosubStmt>(result.lines[0].statement));
    CHECK(std::get<BasicAst::GosubStmt>(result.lines[0].statement).targetLine == 100);
}

TEST_CASE("basic statement parser parses RETURN") {
    BasicProgram program;
    program.setLine(10, "RETURN");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    CHECK(std::holds_alternative<BasicAst::ReturnStmt>(result.lines[0].statement));
}

TEST_CASE("basic statement parser parses CHAIN") {
    BasicProgram program;
    program.setLine(10, "CHAIN \"NEXT\"");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::ChainStmt>(result.lines[0].statement));
}

TEST_CASE("basic statement parser validates CHAIN expression") {
    BasicProgram missing;
    missing.setLine(10, "CHAIN");
    const BasicAst::StatementParseResult missingResult = BasicStatementParser::parse(missing);
    CHECK_FALSE(missingResult.success);
    REQUIRE(missingResult.errors.size() == 1);
    CHECK(missingResult.errors[0].message == "CHAIN requires a program name expression");

    BasicProgram malformed;
    malformed.setLine(10, "CHAIN \"A");
    const BasicAst::StatementParseResult malformedResult = BasicStatementParser::parse(malformed);
    CHECK_FALSE(malformedResult.success);
    REQUIRE(malformedResult.errors.size() == 1);
    CHECK(malformedResult.errors[0].message.find("CHAIN program expression error:") == 0);
}

TEST_CASE("basic statement parser rejects GOSUB without a line number") {
    BasicProgram program;
    program.setLine(10, "GOSUB");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 10);
    CHECK(result.errors[0].message == "GOSUB requires a line number");
}

TEST_CASE("basic statement parser rejects GOSUB with non-numeric target") {
    BasicProgram program;
    program.setLine(10, "GOSUB ABC");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].message == "GOSUB requires a line number");
}

TEST_CASE("basic statement parser rejects RETURN with arguments") {
    BasicProgram program;
    program.setLine(10, "RETURN 99");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 10);
    CHECK(result.errors[0].message == "RETURN takes no arguments");
}

TEST_CASE("basic statement parser parses FOR...TO") {
    BasicProgram program;
    program.setLine(10, "FOR I = 1 TO 10");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::ForStmt>(result.lines[0].statement));
    const auto &stmt = std::get<BasicAst::ForStmt>(result.lines[0].statement);
    CHECK(stmt.variableName == "I");
    CHECK(stmt.initExpr != nullptr);
    CHECK(stmt.limitExpr != nullptr);
    CHECK(stmt.stepExpr == nullptr);
}

TEST_CASE("basic statement parser parses FOR...TO STEP") {
    BasicProgram program;
    program.setLine(10, "FOR I = 1 TO 10 STEP 2");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::ForStmt>(result.lines[0].statement));
    const auto &stmt = std::get<BasicAst::ForStmt>(result.lines[0].statement);
    CHECK(stmt.variableName == "I");
    CHECK(stmt.initExpr != nullptr);
    CHECK(stmt.limitExpr != nullptr);
    CHECK(stmt.stepExpr != nullptr);
}

TEST_CASE("basic statement parser parses NEXT") {
    BasicProgram program;
    program.setLine(20, "NEXT I");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::NextStmt>(result.lines[0].statement));
    CHECK(std::get<BasicAst::NextStmt>(result.lines[0].statement).variableName == "I");
}

TEST_CASE("basic statement parser rejects FOR without TO") {
    BasicProgram program;
    program.setLine(10, "FOR I = 1 5");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 10);
    CHECK(result.errors[0].message == "FOR requires TO");
}

TEST_CASE("basic statement parser rejects FOR without variable") {
    BasicProgram program;
    program.setLine(10, "FOR = 1 TO 10");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 10);
}

TEST_CASE("basic statement parser rejects NEXT without variable") {
    BasicProgram program;
    program.setLine(20, "NEXT");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::NextStmt>(result.lines[0].statement));
    CHECK(std::get<BasicAst::NextStmt>(result.lines[0].statement).variableName.empty());
}

TEST_CASE("basic statement parser parses DIM") {
    BasicProgram program;
    program.setLine(10, "DIM A(10)");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::DimStmt>(result.lines[0].statement));
    const auto &stmt = std::get<BasicAst::DimStmt>(result.lines[0].statement);
    CHECK(stmt.variableName == "A");
    CHECK(stmt.sizeExpr != nullptr);
}

TEST_CASE("basic statement parser parses LET with array subscript") {
    BasicProgram program;
    program.setLine(10, "LET A(I) = 5");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::LetArrayStmt>(result.lines[0].statement));
    const auto &stmt = std::get<BasicAst::LetArrayStmt>(result.lines[0].statement);
    CHECK(stmt.variableName == "A");
    CHECK(stmt.indexExpr != nullptr);
    CHECK(stmt.valueExpr != nullptr);
}

TEST_CASE("basic statement parser rejects DIM without opening paren") {
    BasicProgram program;
    program.setLine(10, "DIM A 10");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 10);
    CHECK(result.errors[0].message == "DIM requires '(' after variable name");
}

TEST_CASE("basic statement parser rejects DIM with empty size") {
    BasicProgram program;
    program.setLine(10, "DIM A()");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 10);
    CHECK(result.errors[0].message == "DIM size expression cannot be empty");
}

TEST_CASE("basic statement parser parses CLEAR") {
    BasicProgram program;
    program.setLine(10, "CLEAR");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    CHECK(std::holds_alternative<BasicAst::ClearStmt>(result.lines[0].statement));
}

TEST_CASE("basic statement parser rejects CLEAR with arguments") {
    BasicProgram program;
    program.setLine(10, "CLEAR 99");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 10);
    CHECK(result.errors[0].message == "CLEAR takes no arguments");
}

TEST_CASE("basic statement parser parses OPEN with ELSE line") {
    BasicProgram program;
    program.setLine(10, "OPEN \"CUSTOMERS\" TO FVAR ELSE 90");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::OpenStmt>(result.lines[0].statement));
    const auto &stmt = std::get<BasicAst::OpenStmt>(result.lines[0].statement);
    CHECK(stmt.fileVar == "FVAR");
    REQUIRE(stmt.elseArm.has_value());
    REQUIRE(stmt.elseArm->line.has_value());
    CHECK(*stmt.elseArm->line == 90);
}

TEST_CASE("basic statement parser parses READ WRITE and CLOSE") {
    BasicProgram program;
    program.setLine(10, "READ REC FROM FVAR, ID ELSE 40");
    program.setLine(20, "WRITE REC ON FVAR, ID");
    program.setLine(30, "CLOSE FVAR");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 3);
    CHECK(std::holds_alternative<BasicAst::ReadStmt>(result.lines[0].statement));
    CHECK(std::holds_alternative<BasicAst::WriteStmt>(result.lines[1].statement));
    CHECK(std::holds_alternative<BasicAst::CloseStmt>(result.lines[2].statement));
}

TEST_CASE("basic statement parser parses READNEXT READV and WRITEV") {
    BasicProgram program;
    program.setLine(10, "READNEXT ID FROM FVAR ELSE 90");
    program.setLine(20, "READV NAME FROM FVAR, ID, 1");
    program.setLine(30, "READV PHONE FROM FVAR, ID, 2, 1 ELSE STOP");
    program.setLine(40, "WRITEV \"NEW\" ON FVAR, ID, 2");
    program.setLine(50, "WRITEV \"ALT\" ON FVAR, ID, 2, 2 ELSE 120");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 5);

    REQUIRE(std::holds_alternative<BasicAst::ReadNextStmt>(result.lines[0].statement));
    const auto &readNext = std::get<BasicAst::ReadNextStmt>(result.lines[0].statement);
    CHECK(readNext.targetVar == "ID");
    CHECK(readNext.fileVar == "FVAR");
    REQUIRE(readNext.elseArm.has_value());

    REQUIRE(std::holds_alternative<BasicAst::ReadVStmt>(result.lines[1].statement));
    const auto &readVAttr = std::get<BasicAst::ReadVStmt>(result.lines[1].statement);
    CHECK(readVAttr.valueIndexExpr == nullptr);

    REQUIRE(std::holds_alternative<BasicAst::ReadVStmt>(result.lines[2].statement));
    const auto &readVSubvalue = std::get<BasicAst::ReadVStmt>(result.lines[2].statement);
    CHECK(readVSubvalue.valueIndexExpr != nullptr);
    REQUIRE(readVSubvalue.elseArm.has_value());

    REQUIRE(std::holds_alternative<BasicAst::WriteVStmt>(result.lines[3].statement));
    const auto &writeVAttr = std::get<BasicAst::WriteVStmt>(result.lines[3].statement);
    CHECK(writeVAttr.valueIndexExpr == nullptr);

    REQUIRE(std::holds_alternative<BasicAst::WriteVStmt>(result.lines[4].statement));
    const auto &writeVSubvalue = std::get<BasicAst::WriteVStmt>(result.lines[4].statement);
    CHECK(writeVSubvalue.valueIndexExpr != nullptr);
    REQUIRE(writeVSubvalue.elseArm.has_value());
}

TEST_CASE("basic statement parser parses READV/WRITEV DICT<token> attributes") {
    BasicProgram program;
    program.setLine(10, "READV NAME FROM FVAR, ID, DICT<PHONE>");
    program.setLine(20, "WRITEV \"NEW\" ON FVAR, ID, DICT<PHONE>");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.lines.size() == 2);

    REQUIRE(std::holds_alternative<BasicAst::ReadVStmt>(result.lines[0].statement));
    const auto &readV = std::get<BasicAst::ReadVStmt>(result.lines[0].statement);
    REQUIRE(readV.attrExpr != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::DictFieldExpr>(readV.attrExpr->node));
    CHECK(std::get<BasicAst::DictFieldExpr>(readV.attrExpr->node).token == "PHONE");

    REQUIRE(std::holds_alternative<BasicAst::WriteVStmt>(result.lines[1].statement));
    const auto &writeV = std::get<BasicAst::WriteVStmt>(result.lines[1].statement);
    REQUIRE(writeV.attrExpr != nullptr);
    REQUIRE(std::holds_alternative<BasicAst::DictFieldExpr>(writeV.attrExpr->node));
    CHECK(std::get<BasicAst::DictFieldExpr>(writeV.attrExpr->node).token == "PHONE");
}

TEST_CASE("basic statement parser accepts OPEN ELSE with single statement") {
    BasicProgram program;
    program.setLine(10, "OPEN \"CUSTOMERS\" TO FVAR ELSE STOP");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    REQUIRE(result.success);
    REQUIRE(result.errors.empty());
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::OpenStmt>(result.lines[0].statement));
    const auto &stmt = std::get<BasicAst::OpenStmt>(result.lines[0].statement);
    REQUIRE(stmt.elseArm.has_value());
    CHECK_FALSE(stmt.elseArm->line.has_value());
    REQUIRE(stmt.elseArm->inlineStatement != nullptr);
    CHECK(std::holds_alternative<BasicAst::StopStmt>(stmt.elseArm->inlineStatement->statement));
}

TEST_CASE("basic statement parser rejects IF branch with multiple statements") {
    BasicProgram program;
    program.setLine(10, "IF A=1 THEN STOP: PRINT 1");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].message == "IF THEN supports exactly one statement");
}

TEST_CASE("basic statement parser rejects FOR and NEXT in branch arms") {
    BasicProgram program;
    program.setLine(10, "IF A=1 THEN FOR I=1 TO 10");
    program.setLine(20, "OPEN \"CUSTOMERS\" TO FVAR ELSE NEXT I");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 2);
    CHECK(result.errors[0].message == "IF THEN does not allow FOR/NEXT");
    CHECK(result.errors[1].message == "OPEN ELSE does not allow FOR/NEXT");
}

TEST_CASE("basic statement parser rejects assignment and input to session @ system variables") {
    BasicProgram program;
    program.setLine(10, "LET @ACCOUNT = 1");
    program.setLine(20, "INPUT @USERNO");
    program.setLine(30, "FOR @LOGNAME = 1 TO 2");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 3);
    CHECK(result.errors[0].message == "Read-only system variable '@ACCOUNT'");
    CHECK(result.errors[1].message == "Read-only system variable '@USERNO'");
    CHECK(result.errors[2].message == "Read-only system variable '@LOGNAME'");
}

TEST_CASE("basic statement parser parses MAT scalar assignment") {
    BasicProgram program;
    program.setLine(10, "MAT A = 0");
    program.setLine(20, "MAT B$ = \"\"");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK(result.success);
    REQUIRE(result.lines.size() == 2);
    REQUIRE(std::holds_alternative<BasicAst::MatAssignStmt>(result.lines[0].statement));
    const auto &a = std::get<BasicAst::MatAssignStmt>(result.lines[0].statement);
    CHECK(a.targetArray == "A");
    CHECK(a.rhsExpr != nullptr);
    CHECK(a.rhsSourceArray.empty());
    const auto &b = std::get<BasicAst::MatAssignStmt>(result.lines[1].statement);
    CHECK(b.targetArray == "B$");
    CHECK(b.rhsExpr != nullptr);
    CHECK(b.rhsSourceArray.empty());
}

TEST_CASE("basic statement parser parses MAT A = MAT B copy") {
    BasicProgram program;
    program.setLine(10, "MAT A = MAT B");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK(result.success);
    REQUIRE(result.lines.size() == 1);
    REQUIRE(std::holds_alternative<BasicAst::MatAssignStmt>(result.lines[0].statement));
    const auto &stmt = std::get<BasicAst::MatAssignStmt>(result.lines[0].statement);
    CHECK(stmt.targetArray == "A");
    CHECK(stmt.rhsExpr == nullptr);
    CHECK(stmt.rhsSourceArray == "B");
}

TEST_CASE("basic statement parser parses MAT READ with and without ELSE") {
    BasicProgram program;
    program.setLine(10, "MAT A = 0");
    program.setLine(20, "MAT READ A FROM FVAR, \"KEY\"");
    program.setLine(30, "MAT READ A FROM FVAR, \"KEY\" ELSE 10");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK(result.success);
    REQUIRE(result.lines.size() == 3);
    REQUIRE(std::holds_alternative<BasicAst::MatReadStmt>(result.lines[1].statement));
    const auto &plain = std::get<BasicAst::MatReadStmt>(result.lines[1].statement);
    CHECK(plain.targetArray == "A");
    CHECK(plain.fileVar == "FVAR");
    CHECK_FALSE(plain.elseArm.has_value());
    REQUIRE(std::holds_alternative<BasicAst::MatReadStmt>(result.lines[2].statement));
    const auto &withElse = std::get<BasicAst::MatReadStmt>(result.lines[2].statement);
    CHECK(withElse.targetArray == "A");
    CHECK(withElse.elseArm.has_value());
}

TEST_CASE("basic statement parser parses MAT WRITE with and without ELSE") {
    BasicProgram program;
    program.setLine(10, "MAT WRITE A ON FVAR, \"KEY\"");
    program.setLine(20, "MAT WRITE A ON FVAR, \"KEY\" ELSE 10");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK(result.success);
    REQUIRE(result.lines.size() == 2);
    REQUIRE(std::holds_alternative<BasicAst::MatWriteStmt>(result.lines[0].statement));
    const auto &plain = std::get<BasicAst::MatWriteStmt>(result.lines[0].statement);
    CHECK(plain.sourceArray == "A");
    CHECK(plain.fileVar == "FVAR");
    CHECK_FALSE(plain.elseArm.has_value());
    REQUIRE(std::holds_alternative<BasicAst::MatWriteStmt>(result.lines[1].statement));
    CHECK(std::get<BasicAst::MatWriteStmt>(result.lines[1].statement).elseArm.has_value());
}

TEST_CASE("basic statement parser rejects malformed MAT statements") {
    BasicProgram program;
    program.setLine(10, "MAT");
    program.setLine(20, "MAT A");
    program.setLine(30, "MAT READ");
    program.setLine(40, "MAT WRITE");
    program.setLine(50, "MAT A = MAT B + MAT C");
    program.setLine(60, "MAT A READ FROM F, X");

    const BasicAst::StatementParseResult result = BasicStatementParser::parse(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 6);
    CHECK(result.errors[0].message.find("MAT") != std::string::npos);
    CHECK(result.errors[1].message.find("MAT") != std::string::npos);
    CHECK(result.errors[2].message.find("MAT READ") != std::string::npos);
    CHECK(result.errors[3].message.find("MAT WRITE") != std::string::npos);
    CHECK(result.errors[4].message.find("MAT copy source must be a bare array name") != std::string::npos);
    CHECK(result.errors[5].message.find("MAT") != std::string::npos);
}

