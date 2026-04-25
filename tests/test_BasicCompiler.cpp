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

TEST_CASE("basic compiler compiles GOSUB and RETURN") {
    BasicProgram program;
    program.setLine(10, "GOSUB 40");
    program.setLine(20, "PRINT 1");
    program.setLine(30, "END");
    program.setLine(40, "PRINT 2");
    program.setLine(50, "RETURN");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    REQUIRE(result.success);
    CHECK(result.errors.empty());
    // Line 10: CALL <ip of line 40>
    CHECK(result.program[0].op == OpCode::Call);
    // Line 20: PushInt 1, PrintVal, PrintEol
    CHECK(result.program[1].op == OpCode::PushInt);
    CHECK(result.program[2].op == OpCode::PrintVal);
    CHECK(result.program[3].op == OpCode::PrintEol);
    // Line 30: HALT (END)
    CHECK(result.program[4].op == OpCode::Halt);
    // Line 40: PushInt 2, PrintVal, PrintEol
    CHECK(result.program[5].op == OpCode::PushInt);
    CHECK(result.program[6].op == OpCode::PrintVal);
    CHECK(result.program[7].op == OpCode::PrintEol);
    // Line 50: RETURN
    CHECK(result.program[8].op == OpCode::Return);
    // CALL operand must resolve to ip of line 40 = 5
    CHECK(std::get<int>(result.program[0].operand) == 5);
}

TEST_CASE("basic compiler rejects GOSUB to unknown line") {
    BasicProgram program;
    program.setLine(10, "GOSUB 99");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors[0].line == 10);
    CHECK(result.errors[0].message == "Unknown target line 99");
}

TEST_CASE("basic compiler compiles simple FOR/NEXT loop") {
    BasicProgram program;
    program.setLine(10, "FOR I = 1 TO 3");
    program.setLine(20, "PRINT I");
    program.setLine(30, "NEXT I");
    program.setLine(40, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    REQUIRE(result.success);
    // FOR I=1 TO 3:
    //   ip0: PUSH_INT 1
    //   ip1: STORE_VAR I
    //   ip2: PUSH_INT 3
    //   ip3: PUSH_INT 1  (default step)
    //   ip4: FOR_SETUP I
    // PRINT I:
    //   ip5: LOAD_VAR I
    //   ip6: PRINT_VAL
    //   ip7: PRINT_EOL
    // NEXT I:
    //   ip8: FOR_NEXT I
    // END:
    //   ip9: HALT
    REQUIRE(result.program.size() == 10);
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(result.program[1].op == OpCode::StoreVar);
    CHECK(result.program[2].op == OpCode::PushInt);
    CHECK(result.program[3].op == OpCode::PushInt);  // step=1
    CHECK(result.program[4].op == OpCode::ForSetup);
    CHECK(std::get<std::string>(result.program[4].operand) == "I");
    CHECK(result.program[5].op == OpCode::LoadVar);
    CHECK(result.program[6].op == OpCode::PrintVal);
    CHECK(result.program[7].op == OpCode::PrintEol);
    CHECK(result.program[8].op == OpCode::ForNext);
    CHECK(std::get<std::string>(result.program[8].operand) == "I");
    CHECK(result.program[9].op == OpCode::Halt);
}

TEST_CASE("basic compiler compiles FOR/NEXT with STEP 2") {
    BasicProgram program;
    program.setLine(10, "FOR I = 0 TO 8 STEP 2");
    program.setLine(20, "NEXT I");
    program.setLine(30, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    REQUIRE(result.success);
    // step comes from PUSH_INT 2 at index 3
    CHECK(result.program[3].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[3].operand) == 2);
    CHECK(result.program[4].op == OpCode::ForSetup);
}

TEST_CASE("basic compiler compiles FOR/NEXT with negative STEP") {
    BasicProgram program;
    program.setLine(10, "FOR I = 5 TO 1 STEP -1");
    program.setLine(20, "NEXT I");
    program.setLine(30, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    REQUIRE(result.success);
    // Just verify a ForSetup with varName "I" appears in the output
    bool foundForSetup = false;
    for (const auto &instr : result.program) {
        if (instr.op == OpCode::ForSetup && std::get<std::string>(instr.operand) == "I") {
            foundForSetup = true;
            break;
        }
    }
    CHECK(foundForSetup);
    bool foundForNext = false;
    for (const auto &instr : result.program) {
        if (instr.op == OpCode::ForNext && std::get<std::string>(instr.operand) == "I") {
            foundForNext = true;
            break;
        }
    }
    CHECK(foundForNext);
}

TEST_CASE("basic compiler rejects $ variable as FOR loop variable") {
    BasicProgram program;
    program.setLine(10, "FOR A$ = 1 TO 5");
    program.setLine(20, "NEXT A$");
    program.setLine(30, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() >= 1);
    CHECK(result.errors[0].line == 10);
}

TEST_CASE("basic compiler compiles DIM, LET array, and PRINT array subscript") {
    BasicProgram program;
    program.setLine(10, "DIM A(5)");
    program.setLine(20, "LET A(3) = 42");
    program.setLine(30, "PRINT A(3)");
    program.setLine(40, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    REQUIRE(result.success);

    // DIM: PUSH_INT 5, DIM_ARRAY A
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[0].operand) == 5);
    CHECK(result.program[1].op == OpCode::DimArray);
    CHECK(std::get<std::string>(result.program[1].operand) == "A");
    // LET A(3)=42: PUSH_INT 42, PUSH_INT 3, STORE_ARR A
    CHECK(result.program[2].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[2].operand) == 42);
    CHECK(result.program[3].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[3].operand) == 3);
    CHECK(result.program[4].op == OpCode::StoreArr);
    CHECK(std::get<std::string>(result.program[4].operand) == "A");
    // PRINT A(3): PUSH_INT 3, LOAD_ARR A, PRINT_VAL, PRINT_EOL
    CHECK(result.program[5].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[5].operand) == 3);
    CHECK(result.program[6].op == OpCode::LoadArr);
    CHECK(std::get<std::string>(result.program[6].operand) == "A");
    CHECK(result.program[7].op == OpCode::PrintVal);
    CHECK(result.program[8].op == OpCode::PrintEol);
    CHECK(result.program[9].op == OpCode::Halt);
}

TEST_CASE("basic compiler applies CoerceInt when writing to % array") {
    BasicProgram program;
    program.setLine(10, "DIM A%(3)");
    program.setLine(20, "LET A%(1) = 7");
    program.setLine(30, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    REQUIRE(result.success);
    // LET A%(1) = 7: PUSH_INT 7, COERCE_INT, PUSH_INT 1, STORE_ARR A%
    CHECK(result.program[2].op == OpCode::PushInt);
    CHECK(result.program[3].op == OpCode::CoerceInt);
    CHECK(result.program[4].op == OpCode::PushInt);
    CHECK(result.program[5].op == OpCode::StoreArr);
}

TEST_CASE("basic compiler compiles CLEAR to a single ClearVars instruction") {
    BasicProgram program;
    program.setLine(10, "CLEAR");
    program.setLine(20, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    REQUIRE(result.success);
    // CLEAR_VARS, HALT
    CHECK(result.program[0].op == OpCode::ClearVars);
    CHECK(result.program[1].op == OpCode::Halt);
}
