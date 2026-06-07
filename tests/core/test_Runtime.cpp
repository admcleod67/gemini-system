#include <doctest/doctest.h>

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <optional>
#include <variant>
#include <vector>
#include <filesystem>

#include "Runtime.h"
#include "FileSystem.h"
#include "LockTable.h"

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

TEST_CASE("runtime push float participates in arithmetic") {
    std::vector<Instruction> prog = {
        {OpCode::PushFlt, 2.5},
        {OpCode::PushInt, 2},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    REQUIRE(std::holds_alternative<double>(rt.stack()[0]));
    CHECK(std::get<double>(rt.stack()[0]) == doctest::Approx(4.5));
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

TEST_CASE("runtime source line defaults to 0 without metadata") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK(rt.currentSourceLine() == 0);
    CHECK(rt.sourceLineAtInstruction(0) == 0);
    CHECK(rt.sourceLineAtInstruction(1) == 0);
    CHECK(rt.sourceLineAtInstruction(99) == 0);
}

TEST_CASE("runtime source line uses optional metadata map") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog, {120, 0});
    CHECK(rt.currentSourceLine() == 120);
    CHECK(rt.step());
    CHECK(rt.currentSourceLine() == 0);
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

TEST_CASE("runtime ADD uses numeric prefix coercion for strings") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"12ABC"}},
        {OpCode::PushInt, 1},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    REQUIRE(std::holds_alternative<double>(rt.stack()[0]));
    CHECK(std::get<double>(rt.stack()[0]) == doctest::Approx(13.0));
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

TEST_CASE("runtime CoerceInt uses numeric prefix same as coerceToInt for 12ABC") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"12ABC"}},
        {OpCode::CoerceInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 12);
}

TEST_CASE("runtime INPUT_STR plus CoerceInt matches trimmed numeric line for percent INPUT") {
    std::istringstream in("  99  \n");
    std::vector<Instruction> prog = {
        {OpCode::InputStr, Value{}},
        {OpCode::CoerceInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setInputStream(&in);
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 99);
}

TEST_CASE("runtime PrintVal output matches STR builtin string for int") {
    std::ostringstream fromPrint;
    Runtime rtPrint;
    rtPrint.setOutputStream(&fromPrint);
    std::vector<Instruction> progPrint = {
        {OpCode::PushInt, 42},
        {OpCode::PrintVal, Value{}},
        {OpCode::Halt, Value{}},
    };
    rtPrint.loadProgram(progPrint);
    rtPrint.run();

    Runtime rtStr;
    std::vector<Instruction> progStr = {
        {OpCode::PushInt, 42},
        {OpCode::InvokeBuiltin, std::string{"STR"}},
        {OpCode::Halt, Value{}},
    };
    rtStr.loadProgram(progStr);
    rtStr.run();
    REQUIRE(rtStr.stack().size() == 1);
    CHECK(fromPrint.str() == std::get<std::string>(rtStr.stack()[0]));
    CHECK(fromPrint.str() == "42");
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

// ─────────────────────── Arrays ─────────────────────────

TEST_CASE("runtime DimArray / StoreArr / LoadArr basic read-write") {
    std::ostringstream out;
    Runtime rt;
    rt.setOutputStream(&out);

    std::vector<Instruction> prog = {
        {OpCode::PushInt,  5},
        {OpCode::DimArray, std::string{"A"}},   // DIM A(5)
        {OpCode::PushInt,  99},                 // value
        {OpCode::PushInt,  3},                  // index
        {OpCode::StoreArr, std::string{"A"}},   // A(3) = 99
        {OpCode::PushInt,  3},                  // index to read
        {OpCode::LoadArr,  std::string{"A"}},   // push A(3)
        {OpCode::PrintVal, Value{}},
        {OpCode::PrintEol, Value{}},
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    CHECK(out.str() == "99\n");
}

TEST_CASE("runtime DimArray initialises elements to empty string") {
    std::ostringstream out;
    Runtime rt;
    rt.setOutputStream(&out);

    std::vector<Instruction> prog = {
        {OpCode::PushInt,  3},
        {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushInt,  1},
        {OpCode::LoadArr,  std::string{"A"}},
        {OpCode::PrintVal, Value{}},
        {OpCode::PrintEol, Value{}},
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    CHECK(out.str() == "\n");
}

TEST_CASE("runtime DimArray reDIM resets elements") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt,  3},
        {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushInt,  7},
        {OpCode::PushInt,  1},
        {OpCode::StoreArr, std::string{"A"}},
        // Re-DIM: should reset
        {OpCode::PushInt,  3},
        {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushInt,  1},
        {OpCode::LoadArr,  std::string{"A"}},
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    // After re-DIM A(3) = "" so coerce to int → 0
    CHECK(rt.stack().size() == 1);
    const auto &top = rt.stack()[0];
    const bool isEmptyStr = std::holds_alternative<std::string>(top) && std::get<std::string>(top).empty();
    CHECK(isEmptyStr);
}

TEST_CASE("runtime LoadArr on undimensioned array throws") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::LoadArr, std::string{"X"}},
        {OpCode::Halt,    Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_WITH(rt.run(), "Array not dimensioned: X");
}

TEST_CASE("runtime StoreArr on undimensioned array throws") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt,  42},
        {OpCode::PushInt,  1},
        {OpCode::StoreArr, std::string{"X"}},
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_WITH(rt.run(), "Array not dimensioned: X");
}

TEST_CASE("runtime LoadArr out-of-bounds throws") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt,  3},
        {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushInt,  5},                  // index 5 > size 3
        {OpCode::LoadArr,  std::string{"A"}},
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_WITH(rt.run(), "Array index out of bounds: A");
}

TEST_CASE("runtime DimArray size less than 1 throws") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt,  0},
        {OpCode::DimArray, std::string{"A"}},
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_WITH(rt.run(), "DIM: size must be >= 1");
}

TEST_CASE("runtime loadProgram clears arrays") {
    Runtime rt;
    std::vector<Instruction> prog1 = {
        {OpCode::PushInt,  3},
        {OpCode::DimArray, std::string{"A"}},
        {OpCode::Halt,     Value{}},
    };
    rt.loadProgram(prog1);
    rt.run();

    // Second run: array should have been cleared
    std::vector<Instruction> prog2 = {
        {OpCode::PushInt, 1},
        {OpCode::LoadArr, std::string{"A"}},
        {OpCode::Halt,    Value{}},
    };
    rt.loadProgram(prog2);
    CHECK_THROWS_WITH(rt.run(), "Array not dimensioned: A");
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

TEST_CASE("runtime ClearVars removes scalar variables") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt,  42},
        {OpCode::StoreVar, std::string{"X"}},
        {OpCode::ClearVars, Value{}},
        // After CLEAR, X is gone — accessing it should throw "Undefined variable"
        {OpCode::LoadVar, std::string{"X"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime ClearVars removes array allocations") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt,  3},
        {OpCode::DimArray, std::string{"A"}},
        {OpCode::ClearVars, Value{}},
        // Now accessing A(1) should throw "Array not dimensioned"
        {OpCode::PushInt, 1},
        {OpCode::LoadArr, std::string{"A"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime AbsInt returns absolute value of positive int") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 5},
        {OpCode::AbsInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 5);
}

TEST_CASE("runtime AbsInt returns absolute value of negative int") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, -7},
        {OpCode::AbsInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 7);
}

TEST_CASE("runtime AbsInt returns 0 for zero") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 0},
        {OpCode::AbsInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
}

TEST_CASE("runtime SgnInt returns 1 for positive") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 42},
        {OpCode::SgnInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
}

TEST_CASE("runtime SgnInt returns -1 for negative") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, -3},
        {OpCode::SgnInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == -1);
}

TEST_CASE("runtime SgnInt returns 0 for zero") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 0},
        {OpCode::SgnInt, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
}

TEST_CASE("runtime SeqStr returns ASCII value of first char") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"A"}},
        {OpCode::SeqStr, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 65);
}

TEST_CASE("runtime SeqStr returns 0 for empty string") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{""}},
        {OpCode::SeqStr, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
}

TEST_CASE("runtime SeqStr uses only first character") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"Hello"}},
        {OpCode::SeqStr, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 72); // 'H'
}

TEST_CASE("runtime SeqStr stringifies integer operand") {
    Runtime rt;
    // SEQ(65) → stringify to "65" → ASCII of '6' = 54
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 65},
        {OpCode::SeqStr, Value{}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 54); // '6'
}

TEST_CASE("runtime InvokeBuiltin ABS matches AbsInt") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, -7},
        {OpCode::InvokeBuiltin, std::string{"ABS"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 7);
}

TEST_CASE("runtime InvokeBuiltin LEN returns byte length") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"abc"}},
        {OpCode::InvokeBuiltin, std::string{"LEN"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 3);
}

TEST_CASE("runtime InvokeBuiltin TRIM strips whitespace") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"  hi  "}},
        {OpCode::InvokeBuiltin, std::string{"TRIM"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "hi");
}

TEST_CASE("runtime InvokeBuiltin SPACE with negative count yields empty string") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, -3},
        {OpCode::InvokeBuiltin, std::string{"SPACE"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]).empty());
}

TEST_CASE("runtime InvokeBuiltin SPACE rejects count above limit") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 65537},
        {OpCode::InvokeBuiltin, std::string{"SPACE"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime InvokeBuiltin unknown name throws with BUILTIN prefix") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::InvokeBuiltin, std::string{"NOT_A_BUILTIN"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    bool threw = false;
    try {
        rt.run();
    } catch (const std::runtime_error &e) {
        threw = true;
        CHECK(std::string(e.what()).find("BUILTIN:") != std::string::npos);
    }
    CHECK(threw);
}

TEST_CASE("runtime InvokeBuiltin INT truncates toward zero") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushFlt, -2.7},
        {OpCode::InvokeBuiltin, std::string{"INT"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == -2);
}

TEST_CASE("runtime InvokeBuiltin MOD uses fmod-style remainder") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 7},
        {OpCode::PushInt, 3},
        {OpCode::InvokeBuiltin, std::string{"MOD"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::abs(std::get<double>(rt.stack()[0]) - 1.0) < 1e-9);
}

TEST_CASE("runtime InvokeBuiltin MOD division by zero throws") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::PushInt, 0},
        {OpCode::InvokeBuiltin, std::string{"MOD"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime InvokeBuiltin LOG rejects non positive") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 0},
        {OpCode::InvokeBuiltin, std::string{"LOG"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime InvokeBuiltin RND with no arguments returns double in zero one") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::InvokeBuiltin, std::string{"RND"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    const double x = std::get<double>(rt.stack()[0]);
    CHECK(x >= 0.0);
    CHECK(x < 1.0);
}

TEST_CASE("runtime InvokeBuiltin SIN of pi over two is one radians") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushFlt, 1.5707963267948966},
        {OpCode::InvokeBuiltin, std::string{"SIN"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::abs(std::get<double>(rt.stack()[0]) - 1.0) < 1e-6);
}

TEST_CASE("runtime InvokeBuiltin DATE returns int not less than pick epoch day") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::InvokeBuiltin, std::string{"DATE"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) >= 732);
}

TEST_CASE("runtime InvokeBuiltin TIME returns eight character HH colon MM colon SS") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::InvokeBuiltin, std::string{"TIME"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    const std::string t = std::get<std::string>(rt.stack()[0]);
    CHECK(t.size() == 8);
    CHECK(t[2] == ':');
    CHECK(t[5] == ':');
}

TEST_CASE("runtime InvokeBuiltin SYSTEM throws when handler not set") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::InvokeBuiltin, std::string{"SYSTEM"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime InvokeBuiltin SYSTEM uses handler when set") {
    Runtime rt;
    rt.setBuiltinSystemCallHandler([](const int n) -> Value {
        if (n == 42) {
            return 99;
        }
        return 0;
    });
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 42},
        {OpCode::InvokeBuiltin, std::string{"SYSTEM"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 99);
}

TEST_CASE("runtime InvokeBuiltin INDEX first occurrence 1 based") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"abcabc"}},
        {OpCode::PushStr, std::string{"abc"}},
        {OpCode::PushInt, 1},
        {OpCode::InvokeBuiltin, std::string{"INDEX"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
}

TEST_CASE("runtime InvokeBuiltin INDEX second occurrence overlapping") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"abcabc"}},
        {OpCode::PushStr, std::string{"abc"}},
        {OpCode::PushInt, 2},
        {OpCode::InvokeBuiltin, std::string{"INDEX"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 4);
}

TEST_CASE("runtime InvokeBuiltin INDEX returns zero when not found") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"abc"}},
        {OpCode::PushStr, std::string{"z"}},
        {OpCode::PushInt, 1},
        {OpCode::InvokeBuiltin, std::string{"INDEX"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
}

TEST_CASE("runtime InvokeBuiltin INDEX rejects empty substring") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"abc"}},
        {OpCode::PushStr, std::string{""}},
        {OpCode::PushInt, 1},
        {OpCode::InvokeBuiltin, std::string{"INDEX"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected throw");
    } catch (const std::runtime_error &e) {
        CHECK(std::string(e.what()).find("BUILTIN: INDEX empty substring") != std::string::npos);
    }
}

TEST_CASE("runtime InvokeBuiltin INDEX rejects occurrence less than one") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"abc"}},
        {OpCode::PushStr, std::string{"a"}},
        {OpCode::PushInt, 0},
        {OpCode::InvokeBuiltin, std::string{"INDEX"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected throw");
    } catch (const std::runtime_error &e) {
        CHECK(std::string(e.what()).find("BUILTIN: INDEX occurrence") != std::string::npos);
    }
}

TEST_CASE("runtime InvokeBuiltin FIELD extracts delimited fields") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"a,b,c"}},
        {OpCode::PushStr, std::string{","}},
        {OpCode::PushInt, 2},
        {OpCode::InvokeBuiltin, std::string{"FIELD"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "b");
}

TEST_CASE("runtime InvokeBuiltin FIELD returns empty past end") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"a,b"}},
        {OpCode::PushStr, std::string{","}},
        {OpCode::PushInt, 9},
        {OpCode::InvokeBuiltin, std::string{"FIELD"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]).empty());
}

TEST_CASE("runtime InvokeBuiltin FIELD rejects empty delimiter") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"ab"}},
        {OpCode::PushStr, std::string{""}},
        {OpCode::PushInt, 1},
        {OpCode::InvokeBuiltin, std::string{"FIELD"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected throw");
    } catch (const std::runtime_error &e) {
        CHECK(std::string(e.what()).find("BUILTIN: FIELD empty delimiter") != std::string::npos);
    }
}

TEST_CASE("runtime InvokeBuiltin FIELD rejects field index less than one") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"a,b"}},
        {OpCode::PushStr, std::string{","}},
        {OpCode::PushInt, 0},
        {OpCode::InvokeBuiltin, std::string{"FIELD"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected throw");
    } catch (const std::runtime_error &e) {
        CHECK(std::string(e.what()).find("BUILTIN: FIELD field index") != std::string::npos);
    }
}

TEST_CASE("runtime InvokeBuiltin STR stringifies int") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 42},
        {OpCode::InvokeBuiltin, std::string{"STR"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "42");
}

TEST_CASE("runtime MatInit broadcasts scalar into every slot of a DIM'd array") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 3},
        {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushInt, 7},
        {OpCode::MatInit, std::string{"A"}},
        {OpCode::PushInt, 1}, {OpCode::LoadArr, std::string{"A"}},
        {OpCode::PushInt, 2}, {OpCode::LoadArr, std::string{"A"}},
        {OpCode::PushInt, 3}, {OpCode::LoadArr, std::string{"A"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 3);
    CHECK(std::get<int>(rt.stack()[0]) == 7);
    CHECK(std::get<int>(rt.stack()[1]) == 7);
    CHECK(std::get<int>(rt.stack()[2]) == 7);
}

TEST_CASE("runtime MatInit coerces string scalar into int slots for a percent-suffix array") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2},
        {OpCode::DimArray, std::string{"A%"}},
        {OpCode::PushStr, std::string{"abc"}},
        {OpCode::MatInit, std::string{"A%"}},
        {OpCode::PushInt, 1}, {OpCode::LoadArr, std::string{"A%"}},
        {OpCode::PushInt, 2}, {OpCode::LoadArr, std::string{"A%"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 2);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
    CHECK(std::get<int>(rt.stack()[1]) == 0);
}

TEST_CASE("runtime MatInit on undefined array throws stable substring") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 0},
        {OpCode::MatInit, std::string{"MISSING"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: MAT undefined array") != std::string::npos);
        CHECK(msg.find("\"MISSING\"") != std::string::npos);
    }
}

TEST_CASE("runtime MatCopy duplicates source slots into destination of matching size") {
    Runtime rt;
    std::vector<Instruction> prog = {
        // DIM A(2); DIM B(2)
        {OpCode::PushInt, 2}, {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushInt, 2}, {OpCode::DimArray, std::string{"B"}},
        // B(1) = "x", B(2) = "y"
        {OpCode::PushStr, std::string{"x"}}, {OpCode::PushInt, 1}, {OpCode::StoreArr, std::string{"B"}},
        {OpCode::PushStr, std::string{"y"}}, {OpCode::PushInt, 2}, {OpCode::StoreArr, std::string{"B"}},
        {OpCode::MatCopy, std::string{"A|B"}},
        {OpCode::PushInt, 1}, {OpCode::LoadArr, std::string{"A"}},
        {OpCode::PushInt, 2}, {OpCode::LoadArr, std::string{"A"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 2);
    CHECK(std::get<std::string>(rt.stack()[0]) == "x");
    CHECK(std::get<std::string>(rt.stack()[1]) == "y");
}

TEST_CASE("runtime MatCopy throws stable substring when sizes differ") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2}, {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushInt, 3}, {OpCode::DimArray, std::string{"B"}},
        {OpCode::MatCopy, std::string{"A|B"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: MAT dimension mismatch") != std::string::npos);
    }
}

TEST_CASE("runtime MatCopy throws when source array is undefined") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2}, {OpCode::DimArray, std::string{"A"}},
        {OpCode::MatCopy, std::string{"A|MISSING"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: MAT undefined array") != std::string::npos);
        CHECK(msg.find("\"MISSING\"") != std::string::npos);
    }
}

TEST_CASE("runtime MatLoadFromRec splits a record into slots when attribute count matches") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 3}, {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushStr, std::string{"alpha\nbeta\ngamma"}},
        {OpCode::MatLoadFromRec, std::string{"A"}},
        {OpCode::PushInt, 1}, {OpCode::LoadArr, std::string{"A"}},
        {OpCode::PushInt, 2}, {OpCode::LoadArr, std::string{"A"}},
        {OpCode::PushInt, 3}, {OpCode::LoadArr, std::string{"A"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 3);
    CHECK(std::get<std::string>(rt.stack()[0]) == "alpha");
    CHECK(std::get<std::string>(rt.stack()[1]) == "beta");
    CHECK(std::get<std::string>(rt.stack()[2]) == "gamma");
}

TEST_CASE("runtime MatLoadFromRec throws stable substring when attribute count is short") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 3}, {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushStr, std::string{"alpha\nbeta"}},
        {OpCode::MatLoadFromRec, std::string{"A"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: MAT READ attribute count mismatch") != std::string::npos);
    }
}

TEST_CASE("runtime MatLoadFromRec throws when attribute count is long") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2}, {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushStr, std::string{"alpha\nbeta\ngamma"}},
        {OpCode::MatLoadFromRec, std::string{"A"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: MAT READ attribute count mismatch") != std::string::npos);
    }
}

TEST_CASE("runtime MatLoadFromRec coerces strings to int for percent-suffix array") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2}, {OpCode::DimArray, std::string{"A%"}},
        {OpCode::PushStr, std::string{"42\n99"}},
        {OpCode::MatLoadFromRec, std::string{"A%"}},
        {OpCode::PushInt, 1}, {OpCode::LoadArr, std::string{"A%"}},
        {OpCode::PushInt, 2}, {OpCode::LoadArr, std::string{"A%"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 2);
    CHECK(std::get<int>(rt.stack()[0]) == 42);
    CHECK(std::get<int>(rt.stack()[1]) == 99);
}

TEST_CASE("runtime MatStoreToRec pushes the array as a raw record string") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 3}, {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushStr, std::string{"alpha"}}, {OpCode::PushInt, 1}, {OpCode::StoreArr, std::string{"A"}},
        {OpCode::PushStr, std::string{"beta"}},  {OpCode::PushInt, 2}, {OpCode::StoreArr, std::string{"A"}},
        {OpCode::PushStr, std::string{"gamma"}}, {OpCode::PushInt, 3}, {OpCode::StoreArr, std::string{"A"}},
        {OpCode::MatStoreToRec, std::string{"A"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "alpha\nbeta\ngamma");
}

TEST_CASE("runtime MatStoreToRec round-trips through MatLoadFromRec") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 3}, {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushStr, std::string{"one"}},   {OpCode::PushInt, 1}, {OpCode::StoreArr, std::string{"A"}},
        {OpCode::PushStr, std::string{"two"}},   {OpCode::PushInt, 2}, {OpCode::StoreArr, std::string{"A"}},
        {OpCode::PushStr, std::string{"three"}}, {OpCode::PushInt, 3}, {OpCode::StoreArr, std::string{"A"}},
        {OpCode::PushInt, 3}, {OpCode::DimArray, std::string{"B"}},
        {OpCode::MatStoreToRec, std::string{"A"}},
        {OpCode::MatLoadFromRec, std::string{"B"}},
        {OpCode::PushInt, 1}, {OpCode::LoadArr, std::string{"B"}},
        {OpCode::PushInt, 2}, {OpCode::LoadArr, std::string{"B"}},
        {OpCode::PushInt, 3}, {OpCode::LoadArr, std::string{"B"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 3);
    CHECK(std::get<std::string>(rt.stack()[0]) == "one");
    CHECK(std::get<std::string>(rt.stack()[1]) == "two");
    CHECK(std::get<std::string>(rt.stack()[2]) == "three");
}

TEST_CASE("runtime MatStoreToRec on undefined array throws stable substring") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::MatStoreToRec, std::string{"MISSING"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: MAT undefined array") != std::string::npos);
        CHECK(msg.find("\"MISSING\"") != std::string::npos);
    }
}

TEST_CASE("runtime MAT WRITE + MAT READ round-trip through filesystem") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-mat-roundtrip-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);
    fs.createFile("ORDERS");

    Runtime rt;
    rt.setFileSystem(&fs);

    std::vector<Instruction> prog = {
        // OPEN "ORDERS" TO FVAR
        {OpCode::PushStr, std::string{"ORDERS"}},
        {OpCode::OpenFile, std::string{"FVAR"}},
        // DIM A(3); fill A(1..3)
        {OpCode::PushInt, 3}, {OpCode::DimArray, std::string{"A"}},
        {OpCode::PushStr, std::string{"one"}},   {OpCode::PushInt, 1}, {OpCode::StoreArr, std::string{"A"}},
        {OpCode::PushStr, std::string{"two"}},   {OpCode::PushInt, 2}, {OpCode::StoreArr, std::string{"A"}},
        {OpCode::PushStr, std::string{"three"}}, {OpCode::PushInt, 3}, {OpCode::StoreArr, std::string{"A"}},
        // MAT WRITE A ON FVAR, "K1"  --> emits MatStoreToRec, then push key, then WriteRec
        {OpCode::MatStoreToRec, std::string{"A"}},
        {OpCode::PushStr, std::string{"K1"}},
        {OpCode::WriteRec, std::string{"FVAR"}},
        // DIM B(3); MAT READ B FROM FVAR, "K1"  --> push key, ReadRec, MatLoadFromRec
        {OpCode::PushInt, 3}, {OpCode::DimArray, std::string{"B"}},
        {OpCode::PushStr, std::string{"K1"}},
        {OpCode::ReadRec, std::string{"FVAR"}},
        {OpCode::MatLoadFromRec, std::string{"B"}},
        {OpCode::PushInt, 1}, {OpCode::LoadArr, std::string{"B"}},
        {OpCode::PushInt, 2}, {OpCode::LoadArr, std::string{"B"}},
        {OpCode::PushInt, 3}, {OpCode::LoadArr, std::string{"B"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 3);
    CHECK(std::get<std::string>(rt.stack()[0]) == "one");
    CHECK(std::get<std::string>(rt.stack()[1]) == "two");
    CHECK(std::get<std::string>(rt.stack()[2]) == "three");
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime InvokeBuiltin OCONV D formats Pick day 732 as 01 Jan 1970") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 732},
        {OpCode::PushStr, std::string{"D"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "01 Jan 1970");
}

TEST_CASE("runtime InvokeBuiltin OCONV D formats Pick day 0 as 31 Dec 1967") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 0},
        {OpCode::PushStr, std::string{"D"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "31 Dec 1967");
}

TEST_CASE("runtime InvokeBuiltin ICONV D parses dd MMM yyyy back to Pick day") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"01 Jan 1970"}},
        {OpCode::PushStr, std::string{"D"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 732);
}

TEST_CASE("runtime InvokeBuiltin ICONV D accepts case-insensitive month name") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"26 may 2026"}},
        {OpCode::PushStr, std::string{"D"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::PushStr, std::string{"D"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "26 May 2026");
}

TEST_CASE("runtime InvokeBuiltin ICONV D rejects garbage with stable error substring") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"not a date"}},
        {OpCode::PushStr, std::string{"D"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: ICONV invalid \"D\" input") != std::string::npos);
    }
}

TEST_CASE("runtime InvokeBuiltin OCONV MT formats seconds since midnight as HH:MM") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 3600},
        {OpCode::PushStr, std::string{"MT"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "01:00");
}

TEST_CASE("runtime InvokeBuiltin OCONV MTS formats seconds since midnight with seconds") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 3661},
        {OpCode::PushStr, std::string{"MTS"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "01:01:01");
}

TEST_CASE("runtime InvokeBuiltin ICONV MT round-trips MT format") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"09:30"}},
        {OpCode::PushStr, std::string{"MT"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 9 * 3600 + 30 * 60);
}

TEST_CASE("runtime InvokeBuiltin ICONV MTS round-trips MTS format") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"23:59:59"}},
        {OpCode::PushStr, std::string{"MTS"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 86399);
}

TEST_CASE("runtime InvokeBuiltin ICONV MT rejects malformed time") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"99:99"}},
        {OpCode::PushStr, std::string{"MT"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: ICONV invalid \"MT\" input") != std::string::npos);
    }
}

TEST_CASE("runtime InvokeBuiltin OCONV MCU uppercases the string") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"hello"}},
        {OpCode::PushStr, std::string{"MCU"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "HELLO");
}

TEST_CASE("runtime InvokeBuiltin OCONV MCL lowercases the string") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"WORLD"}},
        {OpCode::PushStr, std::string{"MCL"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "world");
}

TEST_CASE("runtime InvokeBuiltin OCONV MD2 inserts implicit two decimal places") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 1234},
        {OpCode::PushStr, std::string{"MD2"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "12.34");
}

TEST_CASE("runtime InvokeBuiltin OCONV MD2 keeps leading zeros for small negative") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, -5},
        {OpCode::PushStr, std::string{"MD2"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "-0.05");
}

TEST_CASE("runtime InvokeBuiltin OCONV MD prints integer without decimal") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 7},
        {OpCode::PushStr, std::string{"MD"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "7");
}

TEST_CASE("runtime InvokeBuiltin ICONV MD2 scales decimal back to integer") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"12.34"}},
        {OpCode::PushStr, std::string{"MD2"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1234);
}

TEST_CASE("runtime InvokeBuiltin ICONV MD2 rounds half away from zero") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"1.005"}},
        {OpCode::PushStr, std::string{"MD2"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 101);
}

TEST_CASE("runtime InvokeBuiltin ICONV MD parses plain integer") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"12"}},
        {OpCode::PushStr, std::string{"MD"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 12);
}

TEST_CASE("runtime InvokeBuiltin ICONV MD2 rejects non numeric junk") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"12abc"}},
        {OpCode::PushStr, std::string{"MD2"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: ICONV invalid \"MD2\" input") != std::string::npos);
    }
}

TEST_CASE("runtime InvokeBuiltin OCONV rejects unknown code with stable substring") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 0},
        {OpCode::PushStr, std::string{"D2"}},
        {OpCode::InvokeBuiltin, std::string{"OCONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: OCONV unsupported code") != std::string::npos);
        CHECK(msg.find("\"D2\"") != std::string::npos);
    }
}

TEST_CASE("runtime InvokeBuiltin ICONV rejects unknown code with stable substring") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"x"}},
        {OpCode::PushStr, std::string{"WTF"}},
        {OpCode::InvokeBuiltin, std::string{"ICONV"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        FAIL("expected runtime_error");
    } catch (const std::runtime_error &e) {
        const std::string msg = e.what();
        CHECK(msg.find("BUILTIN: ICONV unsupported code") != std::string::npos);
        CHECK(msg.find("\"WTF\"") != std::string::npos);
    }
}

TEST_CASE("runtime InvokeBuiltin NUM returns 1 for fully numeric string") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"123"}},
        {OpCode::InvokeBuiltin, std::string{"NUM"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
}

TEST_CASE("runtime InvokeBuiltin NUM returns 0 for trailing junk") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"12ABC"}},
        {OpCode::InvokeBuiltin, std::string{"NUM"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
}

TEST_CASE("runtime InvokeBuiltin NUM returns 0 for empty string") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{}},
        {OpCode::InvokeBuiltin, std::string{"NUM"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 0);
}

TEST_CASE("runtime InvokeBuiltin NUM returns 1 for signed decimal") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"-3.14"}},
        {OpCode::InvokeBuiltin, std::string{"NUM"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
}

TEST_CASE("runtime InvokeBuiltin NUM returns 1 for int values") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 42},
        {OpCode::InvokeBuiltin, std::string{"NUM"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
}

TEST_CASE("runtime InvokeBuiltin CONVERT maps chars per position") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"hello"}},
        {OpCode::PushStr, std::string{"el"}},
        {OpCode::PushStr, std::string{"ip"}},
        {OpCode::InvokeBuiltin, std::string{"CONVERT"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "hippo");
}

TEST_CASE("runtime InvokeBuiltin CONVERT drops chars when to is shorter") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"abc"}},
        {OpCode::PushStr, std::string{"b"}},
        {OpCode::PushStr, std::string{}},
        {OpCode::InvokeBuiltin, std::string{"CONVERT"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "ac");
}

TEST_CASE("runtime InvokeBuiltin CONVERT leaves input unchanged when from is empty") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"abc"}},
        {OpCode::PushStr, std::string{}},
        {OpCode::PushStr, std::string{"XYZ"}},
        {OpCode::InvokeBuiltin, std::string{"CONVERT"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "abc");
}

TEST_CASE("runtime OpenFile binds existing file handle") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-openfile-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);
    fs.createFile("CUSTOMERS");

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"CUSTOMERS"}},
        {OpCode::OpenFile, std::string{"FVAR"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    CHECK_NOTHROW(rt.run());
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime OpenFile throws for missing file") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-openfile-missing-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"MISSING"}},
        {OpCode::OpenFile, std::string{"FVAR"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime OpenFileTry pushes success flag") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-openfile-try-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);
    fs.createFile("CUSTOMERS");

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"CUSTOMERS"}},
        {OpCode::OpenFileTry, std::string{"FVAR"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 1);
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime ReadRec and WriteRec use filesystem backend") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readwrite-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);
    fs.createFile("CUST");

    Runtime rt;
    rt.setFileSystem(&fs);

    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"CUST"}},
        {OpCode::OpenFile, std::string{"FVAR"}},
        {OpCode::PushStr, std::string{"VALUE"}},
        {OpCode::PushStr, std::string{"001"}},
        {OpCode::WriteRec, std::string{"FVAR"}},
        {OpCode::PushStr, std::string{"001"}},
        {OpCode::ReadRec, std::string{"FVAR"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "VALUE");
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime CloseFile is no-op for unopened handle") {
    Runtime rt;
    std::vector<Instruction> prog = {
        {OpCode::CloseFile, std::string{"FVAR"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    CHECK_NOTHROW(rt.run());
}

TEST_CASE("runtime system variable reader supplies LoadVar and StoreVar rejects @ names") {
    Runtime rt;
    rt.setSystemVariableReader([](const std::string_view name) -> std::optional<Value> {
        if (name == "@ACCOUNT") {
            return Value{std::string{"ACC1"}};
        }
        return std::nullopt;
    });

    std::vector<Instruction> loadProg = {
        {OpCode::LoadVar, std::string{"@account"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(loadProg);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "ACC1");

    std::vector<Instruction> storeProg = {
        {OpCode::PushInt, 1},
        {OpCode::StoreVar, std::string{"@ACCOUNT"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(storeProg);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
}

TEST_CASE("runtime READNEXT iterates lexicographically and write invalidates cursor") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readnext-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record{"B", "2"});
    fs.write("DATA", PickFS::Record{"A", "1"});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::ReadNext, std::string{"F"}},
        {OpCode::PushStr, std::string{"x"}},
        {OpCode::PushStr, std::string{"C"}},
        {OpCode::WriteRec, std::string{"F"}},
        {OpCode::ReadNext, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };

    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 2);
    CHECK(std::get<std::string>(rt.stack()[0]) == "A");
    CHECK(std::get<std::string>(rt.stack()[1]) == "A");
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime READV and WRITEV use structured attributes") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readv-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record{"ID1", std::string{"NAME\nA"} + static_cast<char>(0xFD) + "B"});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 2},
        {OpCode::ReadV, std::string{"F"}},
        {OpCode::PushStr, std::string{"Z"}},
        {OpCode::PushStr, std::string{"ID1"}},
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 1},
        {OpCode::WriteV, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 1},
        {OpCode::ReadV, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 2);
    CHECK(std::get<std::string>(rt.stack()[0]) == "B");
    CHECK(std::get<std::string>(rt.stack()[1]) == "Z");
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime EXTRACT_ATTR reads attribute and subvalue from raw record variable") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"NAME\nA"} + static_cast<char>(0xFD) + "B"},
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 2},
        {OpCode::ExtractAttr, Value{}},
        {OpCode::PushStr, std::string{"NAME\nA"} + static_cast<char>(0xFD) + "B"},
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 0},
        {OpCode::ExtractAttr, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 2);
    CHECK(std::get<std::string>(rt.stack()[0]) == "B");
    CHECK(std::get<std::string>(rt.stack()[1]) == std::string{"A"} + static_cast<char>(0xFD) + "B");
}

TEST_CASE("runtime READV missing attribute throws ATTRIBUTE.NOT.FOUND") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readv-missing-attr";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    // Only attribute 1 exists.
    fs.write("DATA", PickFS::Record{"ID1", std::string{"NAME"}});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},
        {OpCode::PushInt, 2},   // attrNo (missing)
        {OpCode::PushInt, 0},   // valueIndex == 0 → attribute-level read
        {OpCode::ReadV, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        CHECK(false);
    } catch (const std::runtime_error &e) {
        CHECK(std::string{e.what()}.find("ATTRIBUTE.NOT.FOUND") != std::string::npos);
    }
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime READV missing subvalue throws SUBVALUE.NOT.FOUND") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readv-missing-subvalue";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    // Attribute 1 has subvalues: ["A", ""]. Missing subvalue #3.
    const std::string raw = std::string{"A"} + static_cast<char>(0xFD);
    fs.write("DATA", PickFS::Record{"ID1", raw});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},
        {OpCode::PushInt, 1},  // attrNo
        {OpCode::PushInt, 3},  // valueIndex (missing)
        {OpCode::ReadV, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    try {
        rt.run();
        CHECK(false);
    } catch (const std::runtime_error &e) {
        CHECK(std::string{e.what()}.find("SUBVALUE.NOT.FOUND") != std::string::npos);
    }
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime READV_TRY missing attribute returns empty value and flag=0") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readvtry-missing-attr";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record{"ID1", std::string{"NAME"}});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},
        {OpCode::PushInt, 2}, // attrNo (missing)
        {OpCode::PushInt, 0}, // valueIndex == 0 → attribute-level read
        {OpCode::ReadVTry, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 2);
    CHECK(std::holds_alternative<std::string>(rt.stack()[0]));
    CHECK(std::get<std::string>(rt.stack()[0]).empty());
    CHECK(std::get<int>(rt.stack()[1]) == 0);
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime READV_TRY missing subvalue returns empty value and flag=0") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readvtry-missing-subvalue";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    PickFS::FileSystem fs(root);
    fs.createFile("DATA");
    const std::string raw = std::string{"A"} + static_cast<char>(0xFD);
    fs.write("DATA", PickFS::Record{"ID1", raw});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},
        {OpCode::PushInt, 1}, // attrNo
        {OpCode::PushInt, 3}, // valueIndex (missing)
        {OpCode::ReadVTry, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 2);
    CHECK(std::holds_alternative<std::string>(rt.stack()[0]));
    CHECK(std::get<std::string>(rt.stack()[0]).empty());
    CHECK(std::get<int>(rt.stack()[1]) == 0);
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime ResolveDictAttr resolves DICT token to attribute index") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-resolvedictattr";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    PickFS::FileSystem fs(root);
    fs.createFile("DICT");
    // Minimal type-A DICT row:
    // - attr 1 = "A"
    // - attr 2 = target attribute index ("2" in this test)
    fs.write("DICT", PickFS::Record{"NAME", std::string{"A\n2"}});

    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record{"ID1", std::string{"X\nB"}});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"NAME"}},
        {OpCode::ResolveDictAttr, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 2);
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime READV can read via ResolveDictAttr-resolved attribute index") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readv-dict";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    PickFS::FileSystem fs(root);
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record{"NAME", std::string{"A\n2"}});

    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record{"ID1", std::string{"X\nB"}});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},   // record id
        {OpCode::PushStr, std::string{"NAME"}},  // DICT token
        {OpCode::ResolveDictAttr, std::string{"F"}}, // → attrNo = 2
        {OpCode::PushInt, 0}, // valueIndex == 0 → attribute-level read
        {OpCode::ReadV, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "B");
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime READV_TRY treats unknown DICT token as missing attribute") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readvtry-dict-missing";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    PickFS::FileSystem fs(root);
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record{"NAME", std::string{"A\n2"}});

    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record{"ID1", std::string{"X\nB"}});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},       // record id
        {OpCode::PushStr, std::string{"MISSING"}},   // unknown DICT token
        {OpCode::ResolveDictAttr, std::string{"F"}},  // → attrNo = 0
        {OpCode::PushInt, 0}, // valueIndex == 0 → attribute-level read
        {OpCode::ReadVTry, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 2);
    CHECK(std::get<std::string>(rt.stack()[0]).empty());
    CHECK(std::get<int>(rt.stack()[1]) == 0);
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime ResolveDictAttr pushes token string for F-type field") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-resolvedictattr-f";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    PickFS::FileSystem fs(root);
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record{"THIRD", std::string{"F\n2\n3\n"}});
    fs.createFile("DATA");

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"THIRD"}},
        {OpCode::ResolveDictAttr, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "THIRD");
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime READV evaluates F-type DICT field") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readv-f";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    PickFS::FileSystem fs(root);
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record{"THIRD", std::string{"F\n2\n3\n"}});
    fs.createFile("DATA");
    const std::string body = std::string("X\n") + std::string("A") + static_cast<char>(0xFD) + "B" +
                             static_cast<char>(0xFD) + "C\n";
    fs.write("DATA", PickFS::Record{"ID1", body});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},
        {OpCode::PushStr, std::string{"THIRD"}},
        {OpCode::ResolveDictAttr, std::string{"F"}},
        {OpCode::PushInt, 0},
        {OpCode::ReadV, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "C");
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime READV evaluates I-type DICT field") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readv-i";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    PickFS::FileSystem fs(root);
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record{"NET", std::string{"I\nA + B\n"}});
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record{"ID1", std::string{"10\n5\n"}});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},
        {OpCode::PushStr, std::string{"NET"}},
        {OpCode::ResolveDictAttr, std::string{"F"}},
        {OpCode::PushInt, 0},
        {OpCode::ReadV, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "15");
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime READV computed field rejects value index") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readv-i-vi";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    PickFS::FileSystem fs(root);
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record{"NET", std::string{"I\nA + B\n"}});
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record{"ID1", std::string{"10\n5\n"}});

    Runtime rt;
    rt.setFileSystem(&fs);
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"F"}},
        {OpCode::PushStr, std::string{"ID1"}},
        {OpCode::PushStr, std::string{"NET"}},
        {OpCode::ResolveDictAttr, std::string{"F"}},
        {OpCode::PushInt, 1},
        {OpCode::ReadV, std::string{"F"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    bool threw = false;
    try {
        rt.run();
    } catch (const std::exception &e) {
        threw = true;
        CHECK(std::string(e.what()).find("SUBVALUE.NOT.FOUND") != std::string::npos);
    }
    CHECK(threw);
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime CHAIN throws ChainRequest with program name") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"NEXTPROG"}},
        {OpCode::Chain, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    bool threw = false;
    try {
        rt.run();
    } catch (const PickVM::ChainRequest &req) {
        threw = true;
        CHECK(req.programName == "NEXTPROG");
    }
    CHECK(threw);
}

TEST_CASE("runtime file op taxonomy wording remains classified") {
    Runtime rt;

    {
        std::vector<Instruction> prog = {
            {OpCode::PushStr, std::string{"MISSING"}},
            {OpCode::OpenFile, std::string{"F"}},
            {OpCode::Halt, Value{}},
        };
        const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-taxonomy-open";
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        PickFS::FileSystem fs(root);
        rt.setFileSystem(&fs);
        rt.loadProgram(prog);
        CHECK_THROWS_WITH(rt.run(), doctest::Contains("OPEN: FILE.NOT.FOUND"));
        std::filesystem::remove_all(root, ec);
    }

    {
        std::vector<Instruction> prog = {
            {OpCode::PushStr, std::string{"ID1"}},
            {OpCode::ReadRec, std::string{"NOTOPEN"}},
            {OpCode::Halt, Value{}},
        };
        rt.loadProgram(prog);
        CHECK_THROWS_WITH(rt.run(), doctest::Contains("READ: FILE.VAR.NOT.OPEN"));
    }
}

TEST_CASE("runtime ReadRecU reads record with lock context") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readrecu-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    const auto locks = std::make_shared<PickCore::Locking::LockTable>();
    PickFS::FileSystem fs(root);
    fs.setLockContext(locks, "S1");
    fs.createFile("DATA");
    fs.write("DATA", PickFS::Record("R1", "LOCKED-VAL"));

    Runtime rt;
    rt.setFileSystem(&fs);

    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"FVAR"}},
        {OpCode::PushStr, std::string{"R1"}},
        {OpCode::ReadRecU, std::string{"FVAR"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "LOCKED-VAL");
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime ReadRecU lock conflict throws ERR_RECORD_LOCKED without ON ERROR") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-readrecu-conflict-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    const auto locks = std::make_shared<PickCore::Locking::LockTable>();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    fsA.setLockContext(locks, "S1");
    fsB.setLockContext(locks, "S2");
    fsA.createFile("DATA");
    fsA.write("DATA", PickFS::Record("R1", "VAL"));
    REQUIRE(fsA.readU("DATA", "R1").has_value());

    Runtime rt;
    rt.setFileSystem(&fsB);

    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"FVAR"}},
        {OpCode::PushStr, std::string{"R1"}},
        {OpCode::ReadRecU, std::string{"FVAR"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    CHECK_THROWS_WITH(rt.run(), doctest::Contains("ERR_RECORD_LOCKED"));
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime ON ERROR GOTO routes ReadRecU lock conflict and STATUS returns code") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-onerror-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    const auto locks = std::make_shared<PickCore::Locking::LockTable>();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    fsA.setLockContext(locks, "S1");
    fsB.setLockContext(locks, "S2");
    fsA.createFile("DATA");
    fsA.write("DATA", PickFS::Record("R1", "VAL"));
    REQUIRE(fsA.readU("DATA", "R1").has_value());

    Runtime rt;
    rt.setFileSystem(&fsB);

    std::vector<Instruction> prog = {
        {OpCode::SetOnErrorHandler, 6},
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"FVAR"}},
        {OpCode::PushStr, std::string{"R1"}},
        {OpCode::ReadRecU, std::string{"FVAR"}},
        {OpCode::PushInt, 999},
        {OpCode::InvokeBuiltin, std::string{"STATUS"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == PickVM::kStatusRecordLocked);
    CHECK(rt.statusCode() == PickVM::kStatusRecordLocked);
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime ReleaseRec clears lock so peer ReadRecU succeeds") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-release-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    const auto locks = std::make_shared<PickCore::Locking::LockTable>();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    fsA.setLockContext(locks, "S1");
    fsB.setLockContext(locks, "S2");
    fsA.createFile("DATA");
    fsA.write("DATA", PickFS::Record("R1", "VAL"));
    REQUIRE(fsA.readU("DATA", "R1").has_value());

    Runtime rt;
    rt.setFileSystem(&fsB);

    std::vector<Instruction> blocked = {
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"FVAR"}},
        {OpCode::PushStr, std::string{"R1"}},
        {OpCode::ReadRecU, std::string{"FVAR"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(blocked);
    CHECK_THROWS_WITH(rt.run(), doctest::Contains("ERR_RECORD_LOCKED"));

    fsA.releaseRecord("DATA", "R1");

    rt.loadProgram(blocked);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "VAL");
    std::filesystem::remove_all(root, ec);
}

TEST_CASE("runtime ReadRec lock conflict uses ON ERROR when lock context active") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "pick-runtime-read-lock-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    const auto locks = std::make_shared<PickCore::Locking::LockTable>();
    PickFS::FileSystem fsA(root);
    PickFS::FileSystem fsB(root);
    fsA.setLockContext(locks, "S1");
    fsB.setLockContext(locks, "S2");
    fsA.createFile("DATA");
    fsA.write("DATA", PickFS::Record("R1", "VAL"));
    REQUIRE(fsA.readU("DATA", "R1").has_value());

    Runtime rt;
    rt.setFileSystem(&fsB);

    std::vector<Instruction> prog = {
        {OpCode::SetOnErrorHandler, 6},
        {OpCode::PushStr, std::string{"DATA"}},
        {OpCode::OpenFile, std::string{"FVAR"}},
        {OpCode::PushStr, std::string{"R1"}},
        {OpCode::ReadRec, std::string{"FVAR"}},
        {OpCode::PushInt, 999},
        {OpCode::InvokeBuiltin, std::string{"STATUS"}},
        {OpCode::Halt, Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == PickVM::kStatusRecordLocked);
    std::filesystem::remove_all(root, ec);
}
