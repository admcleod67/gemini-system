#include <doctest/doctest.h>

#include "BasicCompiler.h"

using PickShell::BasicCompiler;
using PickShell::BasicProgram;
using PickVM::OpCode;

TEST_CASE("basic compiler golden arithmetic precedence program") {
    BasicProgram program;
    program.setLine(10, "LET A = 2+3*4");
    program.setLine(20, "PRINT A");
    program.setLine(30, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    CHECK(result.success);
    REQUIRE(result.program.size() == 10);
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(result.program[1].op == OpCode::PushInt);
    CHECK(result.program[2].op == OpCode::PushInt);
    CHECK(result.program[3].op == OpCode::Mul);
    CHECK(result.program[4].op == OpCode::Add);
    CHECK(result.program[5].op == OpCode::StoreVar);
    CHECK(result.program[6].op == OpCode::LoadVar);
    CHECK(result.program[7].op == OpCode::PrintVal);
    CHECK(result.program[8].op == OpCode::PrintEol);
    CHECK(result.program[9].op == OpCode::Halt);
}

TEST_CASE("basic compiler golden if else and goto program") {
    BasicProgram program;
    program.setLine(10, "IF 1 = 1 THEN 30 ELSE 40");
    program.setLine(20, "STOP");
    program.setLine(30, "GOTO 20");
    program.setLine(40, "PRINT 99");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    CHECK(result.success);
    REQUIRE(result.program.size() >= 10);
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(result.program[1].op == OpCode::PushInt);
    CHECK(result.program[2].op == OpCode::Eq);
    CHECK(result.program[3].op == OpCode::JumpIfZero);
    CHECK(result.program[4].op == OpCode::Jump);
    CHECK(result.program[5].op == OpCode::Jump);
    CHECK(result.program[6].op == OpCode::Halt);
    CHECK(result.program[7].op == OpCode::Jump);
}

TEST_CASE("basic compiler golden input print mix program") {
    BasicProgram program;
    program.setLine(10, "INPUT A");
    program.setLine(20, "PRINT A");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    CHECK(result.success);
    REQUIRE(result.program.size() == 6);
    CHECK(result.program[0].op == OpCode::InputStr);
    CHECK(result.program[1].op == OpCode::StoreVar);
    CHECK(result.program[2].op == OpCode::LoadVar);
    CHECK(result.program[3].op == OpCode::PrintVal);
    CHECK(result.program[4].op == OpCode::PrintEol);
    CHECK(result.program[5].op == OpCode::Halt);
}

TEST_CASE("basic compiler golden print string semicolon program") {
    BasicProgram program;
    program.setLine(10, "PRINT \"HELLO\";");
    program.setLine(20, "PRINT 1+1;");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    CHECK(result.success);
    REQUIRE(result.program.size() == 7);
    CHECK(result.program[0].op == OpCode::PushStr);
    CHECK(result.program[1].op == OpCode::PrintVal);
    CHECK(result.program[2].op == OpCode::PushInt);
    CHECK(result.program[3].op == OpCode::PushInt);
    CHECK(result.program[4].op == OpCode::Add);
    CHECK(result.program[5].op == OpCode::PrintVal);
    CHECK(result.program[6].op == OpCode::Halt);
}
