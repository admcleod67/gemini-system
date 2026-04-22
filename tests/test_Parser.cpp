#include <doctest/doctest.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

#include "Parser.h"

using PickVM::LoadedBytecode;
using PickVM::OpCode;
using PickVM::Parser;

TEST_CASE("parser empty stream") {
    Parser parser;
    std::istringstream in("");
    LoadedBytecode lb = parser.parse(in);
    CHECK(lb.program.empty());
}

TEST_CASE("parser comment only line yields no opcodes") {
    Parser parser;
    std::istringstream in("# just a comment\n");
    LoadedBytecode lb = parser.parse(in);
    CHECK(lb.program.empty());
}

TEST_CASE("parser PUSH_INT and HALT") {
    Parser parser;
    std::istringstream in("PUSH_INT 42\nHALT");
    LoadedBytecode lb = parser.parse(in);
    REQUIRE(lb.program.size() == 2);
    CHECK(lb.program[0].op == OpCode::PushInt);
    CHECK(std::get<int>(lb.program[0].operand) == 42);
    CHECK(lb.program[1].op == OpCode::Halt);
    REQUIRE(lb.sourceLinePerInstr.size() == 2);
    CHECK(lb.sourceLinePerInstr[0] == 1);
    CHECK(lb.sourceLinePerInstr[1] == 2);
}

TEST_CASE("parser label and PUSH_STR quoted") {
    Parser parser;
    std::istringstream in("start: PUSH_STR \"hello\"\nHALT");
    LoadedBytecode lb = parser.parse(in);
    REQUIRE(lb.program.size() == 2);
    CHECK(lb.program[0].op == OpCode::PushStr);
    CHECK(std::get<std::string>(lb.program[0].operand) == "hello");
    CHECK(lb.labels.at("start") == 0);
}

TEST_CASE("parser ADD CONCAT PRINT_INT PRINT_STR") {
    Parser parser;
    std::istringstream in(
        "PUSH_INT 1\n"
        "PUSH_INT 2\n"
        "ADD\n"
        "PUSH_STR \"a\"\n"
        "PUSH_STR \"b\"\n"
        "CONCAT\n"
        "PUSH_INT 7\n"
        "PRINT_INT\n"
        "PUSH_STR \"hi\"\n"
        "PRINT_STR\n"
        "HALT\n");
    LoadedBytecode lb = parser.parse(in);
    REQUIRE(lb.program.size() == 11);
    CHECK(lb.program[2].op == OpCode::Add);
    CHECK(lb.program[5].op == OpCode::Concat);
    CHECK(lb.program[7].op == OpCode::PrintInt);
    CHECK(lb.program[9].op == OpCode::PrintStr);
    CHECK(lb.program[10].op == OpCode::Halt);
}

TEST_CASE("parser JUMP and JZ resolved") {
    Parser parser;
    std::istringstream in(
        "PUSH_INT 0\n"
        "JZ skip\n"
        "PUSH_INT 1\n"
        "HALT\n"
        "skip:\n"
        "PUSH_INT 2\n"
        "JUMP end\n"
        "PUSH_INT 99\n"
        "end:\n"
        "HALT\n");
    LoadedBytecode lb = parser.parse(in);
    REQUIRE(lb.program.size() == 8);
    CHECK(lb.program[1].op == OpCode::JumpIfZero);
    CHECK(std::get<int>(lb.program[1].operand) == 4);
    CHECK(lb.program[5].op == OpCode::Jump);
    CHECK(std::get<int>(lb.program[5].operand) == 7);
    CHECK(lb.labels.at("skip") == 4);
    CHECK(lb.labels.at("end") == 7);
}

TEST_CASE("parser label only then opcode") {
    Parser parser;
    std::istringstream in(
        "entry:\n"
        "PUSH_INT 1\n"
        "HALT\n");
    LoadedBytecode lb = parser.parse(in);
    REQUIRE(lb.program.size() == 2);
    CHECK(lb.program[0].op == OpCode::PushInt);
}

TEST_CASE("parser invalid PUSH_INT") {
    Parser parser;
    std::istringstream in("PUSH_INT not_a_number\nHALT\n");
    CHECK_THROWS_AS(parser.parse(in), std::runtime_error);
}

TEST_CASE("parser jump target out of range") {
    Parser parser;
    std::istringstream in(
        "start: PUSH_INT 1\n"
        "JUMP end\n"
        "HALT\n"
        "end:\n");
    CHECK_THROWS_AS(parser.parse(in), std::runtime_error);
}

TEST_CASE("parser unknown opcode") {
    Parser parser;
    std::istringstream in("NOT_AN_OP\n");
    CHECK_THROWS_AS(parser.parse(in), std::runtime_error);
}

TEST_CASE("parser unknown label JUMP") {
    Parser parser;
    std::istringstream in("JUMP nowhere\nHALT\n");
    CHECK_THROWS_AS(parser.parse(in), std::runtime_error);
}

TEST_CASE("parser unknown label JZ") {
    Parser parser;
    std::istringstream in("PUSH_INT 0\nJZ nowhere\nHALT\n");
    CHECK_THROWS_AS(parser.parse(in), std::runtime_error);
}

TEST_CASE("parser DUP DROP SUB") {
    Parser parser;
    std::istringstream in(
        "PUSH_INT 5\n"
        "DUP\n"
        "DROP\n"
        "PUSH_INT 2\n"
        "SUB\n"
        "HALT\n");
    LoadedBytecode lb = parser.parse(in);
    REQUIRE(lb.program.size() == 6);
    CHECK(lb.program[0].op == OpCode::PushInt);
    CHECK(lb.program[1].op == OpCode::Dup);
    CHECK(lb.program[2].op == OpCode::Drop);
    CHECK(lb.program[3].op == OpCode::PushInt);
    CHECK(lb.program[4].op == OpCode::Sub);
    CHECK(lb.program[5].op == OpCode::Halt);
}

TEST_CASE("parser parseFile missing file") {
    Parser parser;
    CHECK_THROWS_AS(parser.parseFile("/nonexistent/pick_system/no_such_file.tbc"), std::runtime_error);
}

TEST_CASE("parser LOAD_VAR and STORE_VAR") {
    Parser parser;
    std::istringstream in(
        "PUSH_INT 10\n"
        "STORE_VAR A\n"
        "LOAD_VAR A\n"
        "PRINT_INT\n"
        "HALT\n");
    LoadedBytecode lb = parser.parse(in);
    REQUIRE(lb.program.size() == 5);
    CHECK(lb.program[1].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(lb.program[1].operand) == "A");
    CHECK(lb.program[2].op == OpCode::LoadVar);
    CHECK(std::get<std::string>(lb.program[2].operand) == "A");
}

TEST_CASE("parser LOAD_VAR and STORE_VAR require names") {
    Parser parser;
    std::istringstream in1("LOAD_VAR\nHALT\n");
    CHECK_THROWS_AS(parser.parse(in1), std::runtime_error);
    std::istringstream in2("STORE_VAR\nHALT\n");
    CHECK_THROWS_AS(parser.parse(in2), std::runtime_error);
}
