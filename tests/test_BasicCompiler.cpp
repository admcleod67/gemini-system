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
