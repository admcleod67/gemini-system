#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <sstream>
#include <stdexcept>
#include <variant>
#include <vector>

#include "Parser.h"
#include "Runtime.h"

using PickVM::Instruction;
using PickVM::OpCode;
using PickVM::Runtime;
using PickVM::Value;

TEST_CASE("parseLine empty and comment") {
    PickVM::Parser parser;
    std::istringstream in("");
    auto prog = parser.parse(in);
    CHECK(prog.empty());
}

TEST_CASE("parse opcode and operand") {
    PickVM::Parser parser;
    std::istringstream in("PUSH_INT 42\nHALT");
    auto prog = parser.parse(in);
    REQUIRE(prog.size() == 2);
    CHECK(prog[0].op == OpCode::PushInt);
    CHECK(std::get<int>(prog[0].operand) == 42);
}

TEST_CASE("parse label and quoted string") {
    PickVM::Parser parser;
    std::istringstream in("start: PUSH_STR \"hello\"\nHALT");
    auto prog = parser.parse(in);
    REQUIRE(prog.size() == 2);
    CHECK(prog[0].op == OpCode::PushStr);
    CHECK(std::get<std::string>(prog[0].operand) == "hello");
}

TEST_CASE("parse minimal program") {
    std::istringstream in(
        "PUSH_INT 2\n"
        "PUSH_INT 3\n"
        "ADD\n"
        "HALT\n");
    PickVM::Parser parser;
    auto prog = parser.parse(in);
    REQUIRE(prog.size() == 4);
    CHECK(prog[0].op == OpCode::PushInt);
    CHECK(std::get<int>(prog[0].operand) == 2);
    CHECK(prog[1].op == OpCode::PushInt);
    CHECK(std::get<int>(prog[1].operand) == 3);
    CHECK(prog[2].op == OpCode::Add);
    CHECK(prog[3].op == OpCode::Halt);
}

TEST_CASE("parse jump target out of range") {
    std::istringstream in(
        "start: PUSH_INT 1\n"
        "JUMP end\n"
        "HALT\n"
        "end:\n");
    PickVM::Parser parser;
    CHECK_THROWS_AS(parser.parse(in), std::runtime_error);
}

TEST_CASE("parse invalid PUSH_INT") {
    std::istringstream in("PUSH_INT not_a_number\nHALT\n");
    PickVM::Parser parser;
    CHECK_THROWS_AS(parser.parse(in), std::runtime_error);
}

TEST_CASE("VM push add halt stack") {
    std::vector<Instruction> prog = {
        {OpCode::PushInt, 2},
        {OpCode::PushInt, 3},
        {OpCode::Add, Value{}},
        {OpCode::Halt, Value{}},
    };
    Runtime vm;
    vm.loadProgram(prog);
    vm.run();
    REQUIRE(vm.stack().size() == 1);
    CHECK(std::get<int>(vm.stack()[0]) == 5);
}
