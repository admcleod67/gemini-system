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
        {OpCode::PushStr, std::string{"ok"}},
        {OpCode::PrintStr, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.setOutputStream(&out);
    rt.loadProgram(prog);
    rt.run();
    CHECK(out.str() == "42\nok\n");
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

TEST_CASE("runtime ADD wrong stack types") {
    std::vector<Instruction> prog = {
        {OpCode::PushStr, std::string{"a"}},
        {OpCode::PushStr, std::string{"b"}},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime rt;
    rt.loadProgram(prog);
    CHECK_THROWS_AS(rt.run(), std::runtime_error);
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
