#include <doctest/doctest.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "Runtime.h"

using PickVM::Instruction;
using PickVM::OpCode;
using PickVM::Runtime;
using PickVM::Value;

TEST_CASE("runtime push int add halt") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 3},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 5);
}

TEST_CASE("runtime step matches run for add program") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 3},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    while (rt.step()) {
    }
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 5);
    CHECK(rt.instructionPointer() >= prog.size());
}

TEST_CASE("runtime empty program step does nothing") {
    Runtime rt;
    rt.loadProgram({});
    CHECK_FALSE(rt.step());
    CHECK(rt.instructionPointer() == 0);
}

TEST_CASE("runtime push str and concat") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"hel"}},
        {OpCode::PushStr, std::string{"lo"}},
        {OpCode::Concat, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "hello");
}

TEST_CASE("runtime print int and str to stream") {
    std::ostringstream out;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 42},
        {OpCode::PrintInt, Value{}},
        {OpCode::PrintEol, Value{}},
        {OpCode::PushStr, std::string{"ok"}},
        {OpCode::PrintStr, Value{}},
        {OpCode::PrintEol, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setOutputStream(&out);
    rt.loadProgram(prog);
    rt.run();
    CHECK(out.str() == "42\nok\n");
}

TEST_CASE("runtime PRINT_INT and PRINT_STR do not append newline") {
    std::ostringstream out;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 42},
        {OpCode::PrintInt, Value{}},
        {OpCode::PushStr, std::string{"ok"}},
        {OpCode::PrintStr, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setOutputStream(&out);
    rt.loadProgram(prog);
    rt.run();
    CHECK(out.str() == "42ok");
}

TEST_CASE("runtime dumpStack uses stream") {
    std::ostringstream out;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setOutputStream(&out);
    rt.loadProgram(prog);
    rt.run();
    rt.dumpStack();
    CHECK(out.str() == "Stack: [ 1 ]\n");
}

TEST_CASE("runtime jump skips push") {
    std::vector<Instruction> prog = {
        {OpCode::Jump, 2},
        {OpCode::PushInt, 99},
        {OpCode::PushInt, 1},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
}

TEST_CASE("runtime JZ jumps when zero") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 0},
        {OpCode::JumpIfZero, 4},
        {OpCode::PushInt, 1},
        {OpCode::Halt, Value{}},
        {OpCode::PushInt, 2},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 2);
}

TEST_CASE("runtime JZ falls through when non-zero") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 3},
        {OpCode::JumpIfZero, 4},
        {OpCode::PushInt, 7},
        {OpCode::Halt, Value{}},
        {OpCode::PushInt, 2},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 7);
}

TEST_CASE("runtime ADD stack underflow") {
    std::vector<Instruction> prog = {
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime ADD non-numeric strings coerce to zero") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"a"}},
        {OpCode::PushStr, std::string{"b"}},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
}

TEST_CASE("runtime CONCAT stack underflow") {
    std::vector<Instruction> prog = {
        {OpCode::Concat, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime CONCAT wrong stack types") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::PushInt, 2},
        {OpCode::Concat, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime PRINT_INT underflow") {
    std::vector<Instruction> prog = {
        {OpCode::PrintInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime PRINT_INT wrong type") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"x"}},
        {OpCode::PrintInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime PushInt malformed operand") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, std::string{"nope"}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime PushStr malformed operand") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, 123},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime DUP then ADD") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::Dup, Value{}},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 2);
}

TEST_CASE("runtime DUP string") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"x"}},
        {OpCode::Dup, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 2);
    CHECK(std::get<std::string>(rt.stack()[0]) == "x");
    CHECK(std::get<std::string>(rt.stack()[1]) == "x");
}

TEST_CASE("runtime DROP") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::PushInt, 2},
        {OpCode::Drop, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
}

TEST_CASE("runtime SUB five minus two") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 5},
        {OpCode::PushInt, 2},
        {OpCode::Sub, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 3);
}

TEST_CASE("runtime SUB stack order is a minus b") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 5},
        {OpCode::Sub, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == -3);
}

TEST_CASE("runtime DUP empty stack") {
    std::vector<Instruction> prog = {
        {OpCode::Dup, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime DROP empty stack") {
    std::vector<Instruction> prog = {
        {OpCode::Drop, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime SUB one value") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::Sub, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime SUB non-numeric strings coerce to zero") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"a"}},
        {OpCode::PushStr, std::string{"b"}},
        {OpCode::Sub, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
}

TEST_CASE("runtime MUL and DIV") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 8},
        {OpCode::PushInt, 2},
        {OpCode::Div, Value{}},
        {OpCode::PushInt, 3},
        {OpCode::Mul, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 12);
}

TEST_CASE("runtime DIV by zero throws") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 8},
        {OpCode::PushInt, 0},
        {OpCode::Div, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime comparison opcodes push integer booleans") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 2},
        {OpCode::Eq, Value{}},
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 3},
        {OpCode::Lt, Value{}},
        {OpCode::PushInt, 3},
        {OpCode::PushInt, 2},
        {OpCode::Ge, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 3);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
    CHECK(std::get<int>(rt.stack()[1]) == 1);
    CHECK(std::get<int>(rt.stack()[2]) == 1);
}

TEST_CASE("runtime comparison opcodes enforce int types") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"x"}},
        {OpCode::PushInt, 1},
        {OpCode::Eq, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime MUL non-numeric string coerces to zero") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"a"}},
        {OpCode::PushInt, 2},
        {OpCode::Mul, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
}

TEST_CASE("runtime STORE_VAR and LOAD_VAR roundtrip int") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 11},
        {OpCode::StoreVar, std::string{"a"}},
        {OpCode::LoadVar, std::string{"A"}},
        {OpCode::PushInt, 1},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 12);
}

TEST_CASE("runtime LOAD_VAR missing throws") {
    std::vector<Instruction> prog = {
        {OpCode::LoadVar, std::string{"MISSING"}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime INPUT_INT reads integer from input stream") {
    std::istringstream in("42\n");
    std::vector<Instruction> prog = {
        {OpCode::InputInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setInputStream(&in);
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 42);
}

TEST_CASE("runtime INPUT_INT invalid input throws") {
    std::istringstream in("xyz\n");
    std::vector<Instruction> prog = {
        {OpCode::InputInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setInputStream(&in);
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime INPUT_INT eof throws") {
    std::istringstream in;
    std::vector<Instruction> prog = {
        {OpCode::InputInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setInputStream(&in);
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime INPUT_STR reads a trimmed line as string") {
    std::istringstream in("  hello world  \n");
    std::vector<Instruction> prog = {
        {OpCode::InputStr, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setInputStream(&in);
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    REQUIRE(std::holds_alternative<std::string>(rt.stack()[0]));
    CHECK(std::get<std::string>(rt.stack()[0]) == "hello world");
}

TEST_CASE("runtime INPUT_STR throws on end of input") {
    std::istringstream in("");
    std::vector<Instruction> prog = {
        {OpCode::InputStr, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setInputStream(&in);
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime string equality comparison") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"hello"}},
        {OpCode::PushStr, std::string{"hello"}},
        {OpCode::Eq, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
}

TEST_CASE("runtime string inequality comparison") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"abc"}},
        {OpCode::PushStr, std::string{"xyz"}},
        {OpCode::Lt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
}

TEST_CASE("runtime comparison throws on type mismatch") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::PushStr, std::string{"hello"}},
        {OpCode::Eq, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime PrintVal prints an integer") {
    std::ostringstream oss;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 42},
        {OpCode::PrintVal, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setOutputStream(&oss);
    rt.loadProgram(prog);
    rt.run();
    CHECK(oss.str() == "42");
}

TEST_CASE("runtime PrintVal prints a string") {
    std::ostringstream oss;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"hello"}},
        {OpCode::PrintVal, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setOutputStream(&oss);
    rt.loadProgram(prog);
    rt.run();
    CHECK(oss.str() == "hello");
}

TEST_CASE("runtime CoerceInt converts numeric string to int") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"99"}},
        {OpCode::CoerceInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 99);
}

TEST_CASE("runtime CoerceInt yields zero for non-numeric string") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"hello"}},
        {OpCode::CoerceInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
}

TEST_CASE("runtime CoerceInt passes through an existing int") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 7},
        {OpCode::CoerceInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 7);
}

TEST_CASE("runtime ADD string plus int coerces string to zero") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"hello"}},
        {OpCode::PushInt, 5},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    // "hello" coerces to 0; 0 + 5 = 5
    CHECK(std::get<int>(rt.stack()[0]) == 5);
}

TEST_CASE("runtime ADD numeric strings coerces both operands") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"10"}},
        {OpCode::PushStr, std::string{"20"}},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 30);
}

TEST_CASE("runtime Call jumps to target and Return resumes after Call") {
    // ip 0: CALL 3     -> push return addr 1, jump to ip 3
    // ip 1: PushInt 10  <- execution resumes here after RETURN
    // ip 2: HALT
    // ip 3: PushInt 99
    // ip 4: DROP        <- discard subroutine result
    // ip 5: RETURN
    std::vector<Instruction> prog = {
        {OpCode::Call, 3},
        {OpCode::PushInt, 10},
        {OpCode::Halt, Value{}},
        {OpCode::PushInt, 99},
        {OpCode::Drop, Value{}},
        {OpCode::Return, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 10);
}

TEST_CASE("runtime nested Call and Return work correctly") {
    // ip 0: CALL 4   -> outer gosub, return addr = 1
    // ip 1: PushInt 1
    // ip 2: HALT
    // ip 3 unused (REM-equivalent): just HALT reachable via jump
    // ip 4: CALL 7   -> inner gosub, return addr = 5
    // ip 5: RETURN   -> returns to ip 1 (outer caller)
    // ip 6: HALT (unreachable)
    // ip 7: PushInt 0
    // ip 8: DROP
    // ip 9: RETURN   -> returns to ip 5
    std::vector<Instruction> prog = {
        {OpCode::Call, 4},     // ip 0
        {OpCode::PushInt, 1},  // ip 1
        {OpCode::Halt, Value{}}, // ip 2
        {OpCode::Halt, Value{}}, // ip 3 (filler)
        {OpCode::Call, 7},     // ip 4: outer subroutine calls inner
        {OpCode::Return, Value{}}, // ip 5: outer RETURN
        {OpCode::Halt, Value{}}, // ip 6 (unreachable)
        {OpCode::PushInt, 0},  // ip 7: inner subroutine
        {OpCode::Drop, Value{}}, // ip 8
        {OpCode::Return, Value{}}, // ip 9: inner RETURN
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
}

TEST_CASE("runtime Return throws when call stack is empty") {
    std::vector<Instruction> prog = {
        {OpCode::Return, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime loadProgram clears call stack between runs") {
    // First run: CALL 2 — pushes a return addr but HALT before RETURN
    std::vector<Instruction> prog1 = {
        {OpCode::Call, 2},
        {OpCode::Halt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog1);
    rt.run();

    // Second run: bare RETURN must throw (call stack was cleared by loadProgram)
    std::vector<Instruction> prog2 = {
        {OpCode::Return, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog2);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

// ─────────────────────── FOR / NEXT ─────────────────────────

TEST_CASE("runtime FOR/NEXT loop body executes at least once (Pick semantics)") {
    // FOR I = 5 TO 1 (no STEP), body should still run once before NEXT increments
    // PUSH_INT 5 → STORE_VAR I → PUSH_INT 1 (limit) → PUSH_INT 1 (step) → FOR_SETUP I
    // body: LOAD_VAR I → PRINT_VAL → PRINT_EOL
    // FOR_NEXT I → HALT
    std::ostringstream out;
    Runtime rt;
    rt.setOutputStream(&out);

    std::vector<Instruction> prog = {
        {OpCode::PushInt,   5},                          // 0 init
        {OpCode::StoreVar,  std::string{"I"}},           // 1
        {OpCode::PushInt,   1},                          // 2 limit
        {OpCode::PushInt,   1},                          // 3 step
        {OpCode::ForSetup,  std::string{"I"}},           // 4 bodyIP = 5
        {OpCode::LoadVar,   std::string{"I"}},           // 5
        {OpCode::PrintVal,  Value{}},                    // 6
        {OpCode::PrintEol,  Value{}},                    // 7
        {OpCode::ForNext,   std::string{"I"}},           // 8
        {OpCode::Halt,      Value{}},                    // 9
    };
    rt.loadProgram(prog);
    rt.run();
    // I starts at 5; NEXT increments to 6; 6 > 1 (limit), so loop exits after one iteration
    const std::string output = out.str();
    CHECK(output == "5\n");
}

TEST_CASE("runtime FOR/NEXT loop executes correct number of times") {
    // FOR I = 1 TO 3 : body : NEXT I
    std::ostringstream out;
    Runtime rt;
    rt.setOutputStream(&out);

    std::vector<Instruction> prog = {
        {OpCode::PushInt,   1},                          // 0 init
        {OpCode::StoreVar,  std::string{"I"}},           // 1
        {OpCode::PushInt,   3},                          // 2 limit
        {OpCode::PushInt,   1},                          // 3 step
        {OpCode::ForSetup,  std::string{"I"}},           // 4 bodyIP = 5
        {OpCode::LoadVar,   std::string{"I"}},           // 5
        {OpCode::PrintVal,  Value{}},                    // 6
        {OpCode::PrintEol,  Value{}},                    // 7
        {OpCode::ForNext,   std::string{"I"}},           // 8
        {OpCode::Halt,      Value{}},                    // 9
    };
    rt.loadProgram(prog);
    rt.run();
    // I=1 body → NEXT → I=2 (2>3? no) → body → NEXT → I=3 (3>3? no) → body → NEXT → I=4 (4>3? yes) → exit
    CHECK(out.str() == "1\n2\n3\n");
}

TEST_CASE("runtime FOR/NEXT loop with STEP 2") {
    std::ostringstream out;
    Runtime rt;
    rt.setOutputStream(&out);

    std::vector<Instruction> prog = {
        {OpCode::PushInt,   1},
        {OpCode::StoreVar,  std::string{"I"}},
        {OpCode::PushInt,   5},
        {OpCode::PushInt,   2},                          // step = 2
        {OpCode::ForSetup,  std::string{"I"}},           // bodyIP = 5
        {OpCode::LoadVar,   std::string{"I"}},
        {OpCode::PrintVal,  Value{}},
        {OpCode::PrintEol,  Value{}},
        {OpCode::ForNext,   std::string{"I"}},
        {OpCode::Halt,      Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    CHECK(out.str() == "1\n3\n5\n");
}

TEST_CASE("runtime FOR/NEXT loop with negative STEP counts down") {
    std::ostringstream out;
    Runtime rt;
    rt.setOutputStream(&out);

    std::vector<Instruction> prog = {
        {OpCode::PushInt,   3},
        {OpCode::StoreVar,  std::string{"I"}},
        {OpCode::PushInt,   1},                          // limit
        {OpCode::PushInt,   -1},                         // step = -1
        {OpCode::ForSetup,  std::string{"I"}},
        {OpCode::LoadVar,   std::string{"I"}},
        {OpCode::PrintVal,  Value{}},
        {OpCode::PrintEol,  Value{}},
        {OpCode::ForNext,   std::string{"I"}},
        {OpCode::Halt,      Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    CHECK(out.str() == "3\n2\n1\n");
}

TEST_CASE("runtime NEXT without FOR throws") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::ForNext, std::string{"I"}},
        {OpCode::Halt,    Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_WITH(rt.run(), "NEXT without FOR");
}

TEST_CASE("runtime FOR/NEXT variable mismatch throws") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt,  1},
        {OpCode::StoreVar, std::string{"I"}},
        {OpCode::PushInt,  5},
        {OpCode::PushInt,  1},
        {OpCode::ForSetup, std::string{"I"}},
        {OpCode::ForNext,  std::string{"J"}},  // mismatch!
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_WITH(rt.run(), "FOR/NEXT variable mismatch");
}

TEST_CASE("runtime STEP 0 throws") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt,  1},
        {OpCode::StoreVar, std::string{"I"}},
        {OpCode::PushInt,  5},
        {OpCode::PushInt,  0},                           // STEP 0
        {OpCode::ForSetup, std::string{"I"}},
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_WITH(rt.run(), "FOR: STEP cannot be zero");
}

TEST_CASE("runtime loadProgram clears forStack") {
    Runtime rt;
    // First run: push a frame but don't NEXT it
    std::vector<Instruction> prog = {
        {OpCode::PushInt,  1},
        {OpCode::StoreVar, std::string{"I"}},
        {OpCode::PushInt,  10},
        {OpCode::PushInt,  1},
        {OpCode::ForSetup, std::string{"I"}},
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog);
    rt.run();

    // Second run: the for stack should have been cleared; bare NEXT should throw
    std::vector<Instruction> prog2 = {
        {OpCode::ForNext, std::string{"I"}},
        {OpCode::Halt,    Value{}},
    };
    rt.loadProgram(prog2);
    CHECK_THROWS_WITH(rt.run(), "NEXT without FOR");
}

TEST_CASE("runtime nested FOR/NEXT loops") {    std::ostringstream out;
    Runtime rt;
    rt.setOutputStream(&out);

    // Outer: I = 1 TO 2; Inner: J = 1 TO 2; PRINT I*10+J
    // ip0:  PUSH_INT 1        outer init
    // ip1:  STORE_VAR I
    // ip2:  PUSH_INT 2        outer limit
    // ip3:  PUSH_INT 1        outer step
    // ip4:  FOR_SETUP I       outer bodyIP = 5
    // ip5:  PUSH_INT 1        inner init
    // ip6:  STORE_VAR J
    // ip7:  PUSH_INT 2        inner limit
    // ip8:  PUSH_INT 1        inner step
    // ip9:  FOR_SETUP J       inner bodyIP = 10
    // ip10: LOAD_VAR I
    // ip11: PUSH_INT 10
    // ip12: MUL
    // ip13: LOAD_VAR J
    // ip14: ADD
    // ip15: PRINT_VAL
    // ip16: PRINT_EOL
    // ip17: FOR_NEXT J
    // ip18: FOR_NEXT I
    // ip19: HALT
    std::vector<Instruction> prog = {
        {OpCode::PushInt,  1},
        {OpCode::StoreVar, std::string{"I"}},
        {OpCode::PushInt,  2},
        {OpCode::PushInt,  1},
        {OpCode::ForSetup, std::string{"I"}},           // ip4, bodyIP=5
        {OpCode::PushInt,  1},
        {OpCode::StoreVar, std::string{"J"}},
        {OpCode::PushInt,  2},
        {OpCode::PushInt,  1},
        {OpCode::ForSetup, std::string{"J"}},           // ip9, bodyIP=10
        {OpCode::LoadVar,  std::string{"I"}},
        {OpCode::PushInt,  10},
        {OpCode::Mul,      Value{}},
        {OpCode::LoadVar,  std::string{"J"}},
        {OpCode::Add,      Value{}},
        {OpCode::PrintVal, Value{}},
        {OpCode::PrintEol, Value{}},
        {OpCode::ForNext,  std::string{"J"}},           // ip17
        {OpCode::ForNext,  std::string{"I"}},           // ip18
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    CHECK(out.str() == "11\n12\n21\n22\n");
}

TEST_CASE("runtime bare NEXT (no variable) matches innermost frame") {    std::ostringstream out;
    Runtime rt;
    rt.setOutputStream(&out);

    // FOR I = 1 TO 3 : PRINT I : NEXT  (no variable name)
    std::vector<Instruction> prog = {
        {OpCode::PushInt,   1},
        {OpCode::StoreVar,  std::string{"I"}},
        {OpCode::PushInt,   3},
        {OpCode::PushInt,   1},
        {OpCode::ForSetup,  std::string{"I"}},           // bodyIP = 5
        {OpCode::LoadVar,   std::string{"I"}},
        {OpCode::PrintVal,  Value{}},
        {OpCode::PrintEol,  Value{}},
        {OpCode::ForNext,   std::string{""}},            // bare NEXT
        {OpCode::Halt,      Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    CHECK(out.str() == "1\n2\n3\n");
}

TEST_CASE("runtime throws UserInterrupt when interrupt flag is set before run") {
    Runtime rt;
    // An infinite jump loop — would never halt on its own.
    std::vector<Instruction> prog = {
        {OpCode::Jump, 0},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.interrupt(); // set flag before run()
    CHECK_THROWS_AS(rt.run(), PickVM::UserInterrupt);
}

TEST_CASE("runtime loadProgram clears interrupt flag") {
    Runtime rt;
    rt.interrupt();
    std::vector<Instruction> prog = {
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog); // should clear the flag
    // run() should complete normally (no UserInterrupt thrown)
    CHECK_NOTHROW(rt.run());
}
