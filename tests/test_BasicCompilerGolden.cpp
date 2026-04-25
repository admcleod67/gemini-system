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

TEST_CASE("basic compiler golden gosub return subroutine program") {
    // 10 GOSUB 50     <- call subroutine at line 50
    // 20 PRINT 1      <- executed after return
    // 30 END
    // 50 PRINT 2      <- subroutine body
    // 60 RETURN
    BasicProgram program;
    program.setLine(10, "GOSUB 50");
    program.setLine(20, "PRINT 1");
    program.setLine(30, "END");
    program.setLine(50, "PRINT 2");
    program.setLine(60, "RETURN");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);
    CHECK(result.success);
    // ip 0: CALL 5
    // ip 1: PUSH_INT 1
    // ip 2: PRINT_VAL
    // ip 3: PRINT_EOL
    // ip 4: HALT
    // ip 5: PUSH_INT 2
    // ip 6: PRINT_VAL
    // ip 7: PRINT_EOL
    // ip 8: RETURN
    // ip 9: HALT (implicit)
    REQUIRE(result.program.size() == 10);
    CHECK(result.program[0].op == OpCode::Call);
    CHECK(std::get<int>(result.program[0].operand) == 5);
    CHECK(result.program[1].op == OpCode::PushInt);
    CHECK(result.program[2].op == OpCode::PrintVal);
    CHECK(result.program[3].op == OpCode::PrintEol);
    CHECK(result.program[4].op == OpCode::Halt);
    CHECK(result.program[5].op == OpCode::PushInt);
    CHECK(result.program[6].op == OpCode::PrintVal);
    CHECK(result.program[7].op == OpCode::PrintEol);
    CHECK(result.program[8].op == OpCode::Return);
    CHECK(result.program[9].op == OpCode::Halt);
}

TEST_CASE("basic compiler golden: simple FOR/NEXT counted loop") {
    // FOR I = 1 TO 3 : PRINT I : NEXT I : END
    // ip0: PUSH_INT 1       (init)
    // ip1: STORE_VAR I
    // ip2: PUSH_INT 3       (limit)
    // ip3: PUSH_INT 1       (default step)
    // ip4: FOR_SETUP I      (bodyIP = 5)
    // ip5: LOAD_VAR I       (body start)
    // ip6: PRINT_VAL
    // ip7: PRINT_EOL
    // ip8: FOR_NEXT I       (increment, test, jump or fall)
    // ip9: HALT
    BasicProgram program;
    program.setLine(10, "FOR I = 1 TO 3");
    program.setLine(20, "PRINT I");
    program.setLine(30, "NEXT I");
    program.setLine(40, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    REQUIRE(result.success);
    REQUIRE(result.program.size() == 10);
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[0].operand) == 1);
    CHECK(result.program[1].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(result.program[1].operand) == "I");
    CHECK(result.program[2].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[2].operand) == 3);
    CHECK(result.program[3].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[3].operand) == 1);
    CHECK(result.program[4].op == OpCode::ForSetup);
    CHECK(std::get<std::string>(result.program[4].operand) == "I");
    CHECK(result.program[5].op == OpCode::LoadVar);
    CHECK(result.program[6].op == OpCode::PrintVal);
    CHECK(result.program[7].op == OpCode::PrintEol);
    CHECK(result.program[8].op == OpCode::ForNext);
    CHECK(std::get<std::string>(result.program[8].operand) == "I");
    CHECK(result.program[9].op == OpCode::Halt);
}

TEST_CASE("basic compiler golden: DIM / LET array / PRINT array subscript") {
    // ip0: PUSH_INT 5        DIM A(5) size
    // ip1: DIM_ARRAY A
    // ip2: PUSH_INT 42       LET A(3) = 42 value
    // ip3: PUSH_INT 3        index
    // ip4: STORE_ARR A
    // ip5: PUSH_INT 3        PRINT A(3) index
    // ip6: LOAD_ARR A
    // ip7: PRINT_VAL
    // ip8: PRINT_EOL
    // ip9: HALT
    BasicProgram program;
    program.setLine(10, "DIM A(5)");
    program.setLine(20, "LET A(3) = 42");
    program.setLine(30, "PRINT A(3)");
    program.setLine(40, "END");

    BasicCompiler compiler;
    const auto result = compiler.compile(program);

    REQUIRE(result.success);
    REQUIRE(result.program.size() == 10);
    CHECK(result.program[0].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[0].operand) == 5);
    CHECK(result.program[1].op == OpCode::DimArray);
    CHECK(std::get<std::string>(result.program[1].operand) == "A");
    CHECK(result.program[2].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[2].operand) == 42);
    CHECK(result.program[3].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[3].operand) == 3);
    CHECK(result.program[4].op == OpCode::StoreArr);
    CHECK(std::get<std::string>(result.program[4].operand) == "A");
    CHECK(result.program[5].op == OpCode::PushInt);
    CHECK(std::get<int>(result.program[5].operand) == 3);
    CHECK(result.program[6].op == OpCode::LoadArr);
    CHECK(std::get<std::string>(result.program[6].operand) == "A");
    CHECK(result.program[7].op == OpCode::PrintVal);
    CHECK(result.program[8].op == OpCode::PrintEol);
    CHECK(result.program[9].op == OpCode::Halt);
}
