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
    CHECK(result.instructionCount == 6);
    CHECK(result.labelCount == 0);
    REQUIRE(result.program.size() == 6);
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(result.program[1].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(result.program[1].operand) == "A");
    CHECK(result.program[2].op == OpCode::LoadVar);
    CHECK(result.program[3].op == OpCode::PrintVal);
    CHECK(result.program[4].op == OpCode::PrintEol);
    CHECK(result.program[5].op == OpCode::Halt);
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
    REQUIRE(result.program.size() == 4);
    CHECK(result.program[0].op == OpCode::PushStr);
    CHECK(std::get<std::string>(result.program[0].operand) == "HELLO");
    CHECK(result.program[1].op == OpCode::PrintVal);
    CHECK(result.program[2].op == OpCode::PrintEol);
    CHECK(result.program[3].op == OpCode::Halt);
}

TEST_CASE("basic compiler supports PRINT trailing semicolon to suppress EOL") {
    BasicProgram program;
    program.setLine(10, "PRINT \"HELLO\";");
    program.setLine(20, "PRINT 1+1;");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    REQUIRE(result.program.size() >= 7);
    CHECK(result.program[0].op == OpCode::PushStr);
    CHECK(result.program[1].op == OpCode::PrintVal);
    CHECK(result.program[2].op == OpCode::PushInt);
    CHECK(result.program[3].op == OpCode::PushInt);
    CHECK(result.program[4].op == OpCode::Add);
    CHECK(result.program[5].op == OpCode::PrintVal);
    CHECK(result.program[6].op == OpCode::Halt);
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

TEST_CASE("basic compiler accepts any expression in LET for dynamic variables") {
    BasicProgram program;
    program.setLine(10, "LET A = \"hello\"");
    program.setLine(20, "PRINT \"OK\"");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    // Pick BASIC allows assigning strings to non-$ variables; numeric coercion happens at use-time.
    CHECK(result.success);
    CHECK(result.errors.empty());
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
    REQUIRE(result.program.size() == 6);
    CHECK(result.program[0].op == OpCode::InputStr);
    CHECK(result.program[1].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(result.program[1].operand) == "A");
    CHECK(result.program[2].op == OpCode::LoadVar);
    CHECK(result.program[3].op == OpCode::PrintVal);
    CHECK(result.program[4].op == OpCode::PrintEol);
    CHECK(result.program[5].op == OpCode::Halt);
}

TEST_CASE("basic compiler REM produces no instructions") {
    BasicProgram program;
    program.setLine(10, "REM initialise counter");
    program.setLine(20, "LET A = 1");
    program.setLine(30, "REM print result");
    program.setLine(40, "PRINT A");
    program.setLine(50, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    // REM lines emit no instructions: PushInt + StoreVar + LoadVar + PrintVal + PrintEol + Halt
    REQUIRE(result.program.size() == 6);
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(result.program[1].op == OpCode::StoreVar);
    CHECK(result.program[2].op == OpCode::LoadVar);
    CHECK(result.program[3].op == OpCode::PrintVal);
    CHECK(result.program[4].op == OpCode::PrintEol);
    CHECK(result.program[5].op == OpCode::Halt);
}

TEST_CASE("basic compiler GOTO can target a REM line") {
    BasicProgram program;
    program.setLine(10, "GOTO 20");
    program.setLine(20, "REM skipped over");
    program.setLine(30, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    // Line 20 (REM) emits nothing, so lineToInstructionIndex[20] == 1
    // which is the same slot as line 30 (END/Halt).
    REQUIRE(result.program.size() == 2);
    CHECK(result.program[0].op == OpCode::Jump);
    CHECK(std::get<int>(result.program[0].operand) == 1);
    CHECK(result.program[1].op == OpCode::Halt);
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

TEST_CASE("basic compiler compiles string variable assignment and print") {
    BasicProgram program;
    program.setLine(10, "LET A$ = \"hello\"");
    program.setLine(20, "PRINT A$");
    program.setLine(30, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    REQUIRE(result.program.size() == 6);
    CHECK(result.program[0].op == OpCode::PushStr);
    CHECK(std::get<std::string>(result.program[0].operand) == "hello");
    CHECK(result.program[1].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(result.program[1].operand) == "A$");
    CHECK(result.program[2].op == OpCode::LoadVar);
    CHECK(std::get<std::string>(result.program[2].operand) == "A$");
    CHECK(result.program[3].op == OpCode::PrintVal);
    CHECK(result.program[4].op == OpCode::PrintEol);
    CHECK(result.program[5].op == OpCode::Halt);
}

TEST_CASE("basic compiler compiles INPUT for string variable") {
    BasicProgram program;
    program.setLine(10, "INPUT A$");
    program.setLine(20, "PRINT A$");
    program.setLine(30, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    REQUIRE(result.program.size() == 6);
    CHECK(result.program[0].op == OpCode::InputStr);
    CHECK(result.program[1].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(result.program[1].operand) == "A$");
    CHECK(result.program[2].op == OpCode::LoadVar);
    CHECK(result.program[3].op == OpCode::PrintVal);
    CHECK(result.program[4].op == OpCode::PrintEol);
    CHECK(result.program[5].op == OpCode::Halt);
}

TEST_CASE("basic compiler compiles string comparison in IF condition") {
    BasicProgram program;
    program.setLine(10, "LET A$ = \"yes\"");
    program.setLine(20, "IF A$ = \"yes\" THEN 40 ELSE 50");
    program.setLine(30, "END");
    program.setLine(40, "PRINT \"match\"");
    program.setLine(50, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
    // After LET A$ (PushStr + StoreVar), IF emits LoadVar A$, PushStr "yes", Eq, JZ, Jump, [JumpElse]
    CHECK(result.program[2].op == OpCode::LoadVar);
    CHECK(std::get<std::string>(result.program[2].operand) == "A$");
    CHECK(result.program[3].op == OpCode::PushStr);
    CHECK(std::get<std::string>(result.program[3].operand) == "yes");
    CHECK(result.program[4].op == OpCode::Eq);
}

TEST_CASE("basic compiler accepts string literal assigned to dynamic variable") {
    BasicProgram program;
    program.setLine(10, "LET A = \"hello\"");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    // Pick BASIC: any value may be stored in a dynamic variable; coercion happens at use-time.
    CHECK(result.success);
    CHECK(result.errors.empty());
}

TEST_CASE("basic compiler accepts integer assigned to string variable") {
    BasicProgram program;
    program.setLine(10, "LET A$ = 42");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    // Pick BASIC: $ is a naming convention; the value 42 is stored as int under A$.
    CHECK(result.success);
    CHECK(result.errors.empty());
}

TEST_CASE("basic compiler compiles LET for percent variable with CoerceInt") {
    BasicProgram program;
    program.setLine(10, "LET A% = 42");
    program.setLine(20, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    REQUIRE(result.success);
    // PushInt, CoerceInt, StoreVar, Halt
    REQUIRE(result.program.size() == 4);
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(result.program[1].op == OpCode::CoerceInt);
    CHECK(result.program[2].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(result.program[2].operand) == "A%");
    CHECK(result.program[3].op == OpCode::Halt);
}

TEST_CASE("basic compiler compiles INPUT for percent variable with CoerceInt") {
    BasicProgram program;
    program.setLine(10, "INPUT A%");
    program.setLine(20, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    REQUIRE(result.success);
    // InputStr, CoerceInt, StoreVar, Halt
    REQUIRE(result.program.size() == 4);
    CHECK(result.program[0].op == OpCode::InputStr);
    CHECK(result.program[1].op == OpCode::CoerceInt);
    CHECK(result.program[2].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(result.program[2].operand) == "A%");
    CHECK(result.program[3].op == OpCode::Halt);
}

TEST_CASE("basic compiler rejects dollar variable in arithmetic expression") {
    BasicProgram program;
    program.setLine(10, "LET A$ = \"hello\"");
    program.setLine(20, "PRINT A$ + 1");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK_FALSE(result.success);
    REQUIRE_FALSE(result.errors.empty());
    CHECK(result.errors[0].line == 20);
}

TEST_CASE("basic compiler accepts percent variable in expression parser") {
    BasicProgram program;
    program.setLine(10, "LET COUNT% = 0");
    program.setLine(20, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK(result.success);
    CHECK(result.errors.empty());
}
