#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <sstream>
#include <stdexcept>
#include <variant>
#include <vector>

#include "Instructions.h"
#include "VM.h"

using PickVM::Instruction;
using PickVM::OpCode;
using PickVM::VM;
using PickVM::Value;

TEST_CASE("parseLine empty and comment") {
    auto a = PickVM::parseLine("");
    CHECK(a.opcode.empty());
    auto b = PickVM::parseLine("   ");
    CHECK(b.opcode.empty());
    auto c = PickVM::parseLine("# comment");
    CHECK(c.opcode.empty());
}

TEST_CASE("parseLine opcode and operand") {
    auto pl = PickVM::parseLine("PUSH_INT 42");
    CHECK(pl.label.empty());
    CHECK(pl.opcode == "PUSH_INT");
    CHECK(pl.operand == "42");
}

TEST_CASE("parseLine label and quoted string") {
    auto pl = PickVM::parseLine("start: PUSH_STR \"hello\"");
    CHECK(pl.label == "start");
    CHECK(pl.opcode == "PUSH_STR");
    CHECK(pl.operand == "\"hello\"");
}

TEST_CASE("loadTextBytecodeStream minimal program") {
    std::istringstream in(
        "PUSH_INT 2\n"
        "PUSH_INT 3\n"
        "ADD\n"
        "HALT\n");
    auto prog = PickVM::loadTextBytecodeStream(in);
    REQUIRE(prog.size() == 4);
    CHECK(prog[0].op == OpCode::PushInt);
    CHECK(std::get<int>(prog[0].operand) == 2);
    CHECK(prog[1].op == OpCode::PushInt);
    CHECK(std::get<int>(prog[1].operand) == 3);
    CHECK(prog[2].op == OpCode::Add);
    CHECK(prog[3].op == OpCode::Halt);
}

TEST_CASE("loadTextBytecodeStream jump target out of range") {
    std::istringstream in(
        "start: PUSH_INT 1\n"
        "JUMP end\n"
        "HALT\n"
        "end:\n");
    CHECK_THROWS_AS(PickVM::loadTextBytecodeStream(in), std::runtime_error);
}

TEST_CASE("loadTextBytecodeStream invalid PUSH_INT") {
    std::istringstream in("PUSH_INT not_a_number\nHALT\n");
    CHECK_THROWS_AS(PickVM::loadTextBytecodeStream(in), std::runtime_error);
}

TEST_CASE("VM push add halt stack") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 3},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    VM vm;
    vm.loadProgram(prog);
    vm.run();
    REQUIRE(vm.stack().size() == 1);
    CHECK(std::get<int>(vm.stack()[0]) == 5);
}
