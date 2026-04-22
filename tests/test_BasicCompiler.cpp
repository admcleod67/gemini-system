#include <doctest/doctest.h>

#include "BasicCompiler.h"

using PickShell::BasicCompiler;
using PickShell::BasicProgram;
using PickVM::OpCode;

TEST_CASE("basic compiler compiles LET PRINT END") {
    BasicProgram program;
    program.setLine(10, "LET A = 5");
    program.setLine(20, "PRINT A");
    program.setLine(30, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    CHECK(result.instructionCount == 5);
    CHECK(result.labelCount == 0);
    REQUIRE(result.program.size() == 5);
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(result.program[1].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(result.program[1].operand) == "A");
    CHECK(result.program[2].op == OpCode::LoadVar);
    CHECK(result.program[3].op == OpCode::PrintInt);
    CHECK(result.program[4].op == OpCode::Halt);
}

TEST_CASE("basic compiler compiles arithmetic precedence and parentheses") {
    BasicProgram program;
    program.setLine(10, "LET A = 2+3*4");
    program.setLine(20, "LET B = (2+3)*4");
    program.setLine(30, "PRINT B");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    REQUIRE(result.program.size() >= 13);
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(result.program[1].op == OpCode::PushInt);
    CHECK(result.program[2].op == OpCode::PushInt);
    CHECK(result.program[3].op == OpCode::Mul);
    CHECK(result.program[4].op == OpCode::Add);
}

TEST_CASE("basic compiler supports unary minus in expressions") {
    BasicProgram program;
    program.setLine(10, "LET A = -5");
    program.setLine(20, "PRINT -A");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(result.program[1].op == OpCode::PushInt);
    CHECK(result.program[2].op == OpCode::Sub);
}

TEST_CASE("basic compiler adds implicit END halt") {
    BasicProgram program;
    program.setLine(10, "LET A = 5");
    program.setLine(20, "PRINT A");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    REQUIRE_FALSE(result.program.empty());
    CHECK(result.program.back().op == OpCode::Halt);
}

TEST_CASE("basic compiler variable names are uppercase") {
    BasicProgram program;
    program.setLine(10, "LET a = 5");
    program.setLine(20, "PRINT a");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    REQUIRE(result.program.size() >= 4);
    CHECK(std::get<std::string>(result.program[1].operand) == "A");
    CHECK(std::get<std::string>(result.program[2].operand) == "A");
}

TEST_CASE("basic compiler supports PRINT string literals") {
    BasicProgram program;
    program.setLine(10, "PRINT \"HELLO\"");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    REQUIRE(result.program.size() == 3);
    CHECK(result.program[0].op == OpCode::PushStr);
    CHECK(std::get<std::string>(result.program[0].operand) == "HELLO");
    CHECK(result.program[1].op == OpCode::PrintStr);
    CHECK(result.program[2].op == OpCode::Halt);
}

TEST_CASE("basic compiler reports unknown keyword with line") {
    BasicProgram program;
    program.setLine(30, "MAT A = 1");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 30);
    CHECK(result.errors[0].message == "Unknown keyword 'MAT'");
}

TEST_CASE("basic compiler enforces int-only LET and PRINT variables") {
    BasicProgram program;
    program.setLine(10, "LET A = \"X\"");
    program.setLine(20, "PRINT \"OK\"");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 10);
}

TEST_CASE("basic compiler reports expression syntax diagnostics") {
    BasicProgram program;
    program.setLine(10, "LET A = 2+");
    program.setLine(20, "PRINT (1+2");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 2);
    CHECK(result.errors[0].line == 10);
    CHECK(result.errors[0].message.find("LET expression error:") == 0);
    CHECK(result.errors[1].line == 20);
    CHECK(result.errors[1].message.find("PRINT expression error:") == 0);
}

TEST_CASE("basic compiler supports relational operators in expressions") {
    BasicProgram program;
    program.setLine(10, "LET A = 2 < 3");
    program.setLine(20, "PRINT 4 >= 5");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    REQUIRE(result.program.size() >= 7);
    CHECK(result.program[2].op == OpCode::Lt);
    CHECK(result.program[6].op == OpCode::Ge);
}

TEST_CASE("basic compiler compiles GOTO STOP and IF THEN ELSE line targets") {
    BasicProgram program;
    program.setLine(10, "IF 1 = 1 THEN 30 ELSE 40");
    program.setLine(20, "STOP");
    program.setLine(30, "GOTO 20");
    program.setLine(40, "PRINT 99");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    REQUIRE(result.program.size() >= 10);
    CHECK(result.program[2].op == OpCode::Eq);
    CHECK(result.program[3].op == OpCode::JumpIfZero);
    CHECK(result.program[4].op == OpCode::Jump);
    CHECK(result.program[5].op == OpCode::Jump);
    CHECK(result.program[6].op == OpCode::Halt);
    CHECK(result.program[7].op == OpCode::Jump);
}

TEST_CASE("basic compiler reports unknown control flow target line") {
    BasicProgram program;
    program.setLine(10, "GOTO 99");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 10);
    CHECK(result.errors[0].message == "Unknown target line 99");
}

TEST_CASE("basic compiler reports malformed IF target syntax") {
    BasicProgram program;
    program.setLine(10, "IF 1 THEN");
    program.setLine(20, "IF 1 THEN 30 ELSE");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 2);
    CHECK(result.errors[0].message == "IF THEN requires a line number");
    CHECK(result.errors[1].message == "IF ELSE requires a line number");
}

TEST_CASE("basic compiler compiles INPUT variable") {
    BasicProgram program;
    program.setLine(10, "INPUT A");
    program.setLine(20, "PRINT A");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    REQUIRE(result.program.size() == 5);
    CHECK(result.program[0].op == OpCode::InputInt);
    CHECK(result.program[1].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(result.program[1].operand) == "A");
    CHECK(result.program[2].op == OpCode::LoadVar);
    CHECK(result.program[3].op == OpCode::PrintInt);
    CHECK(result.program[4].op == OpCode::Halt);
}

TEST_CASE("basic compiler INPUT requires one valid variable name") {
    BasicProgram missingVar;
    missingVar.setLine(10, "INPUT");
    BasicCompiler compiler;
    const auto missingVarResult = compiler.compile(missingVar);
    CHECK_FALSE(missingVarResult.success);
    REQUIRE(missingVarResult.errors.size() == 1);
    CHECK(missingVarResult.errors[0].line == 10);
    CHECK(missingVarResult.errors[0].message == "INPUT requires a variable name");

    BasicProgram extraArgs;
    extraArgs.setLine(10, "INPUT A B");
    const auto extraArgsResult = compiler.compile(extraArgs);
    CHECK_FALSE(extraArgsResult.success);
    REQUIRE(extraArgsResult.errors.size() == 1);
    CHECK(extraArgsResult.errors[0].message == "INPUT requires a variable name");

    BasicProgram invalidVar;
    invalidVar.setLine(10, "INPUT 1A");
    const auto invalidVarResult = compiler.compile(invalidVar);
    CHECK_FALSE(invalidVarResult.success);
    REQUIRE(invalidVarResult.errors.size() == 1);
    CHECK(invalidVarResult.errors[0].message == "Invalid variable name '1A'");
}
