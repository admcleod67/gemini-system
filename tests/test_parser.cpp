#include <doctest/doctest.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

#include "Parser.h"

using PickVM::OpCode;
using PickVM::Parser;

TEST_CASE("parser empty stream") {
    Parser parser;
    std::istringstream in("");
    auto prog = parser.parse(in);
    CHECK(prog.empty());
}

TEST_CASE("parser comment only line yields no opcodes") {
    Parser parser;
    std::istringstream in("# just a comment\n");
    auto prog = parser.parse(in);
    CHECK(prog.empty());
}

TEST_CASE("parser PUSH_INT and HALT") {
    Parser parser;
    std::istringstream in("PUSH_INT 42\nHALT");
    auto prog = parser.parse(in);
    REQUIRE(prog.size() == 2);
    CHECK(prog[0].op == OpCode::PushInt);
    CHECK(std::get<int>(prog[0].operand) == 42);
    CHECK(prog[1].op == OpCode::Halt);
}

TEST_CASE("parser label and PUSH_STR quoted") {
    Parser parser;
    std::istringstream in("start: PUSH_STR \"hello\"\nHALT");
    auto prog = parser.parse(in);
    REQUIRE(prog.size() == 2);
    CHECK(prog[0].op == OpCode::PushStr);
    CHECK(std::get<std::string>(prog[0].operand) == "hello");
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
    auto prog = parser.parse(in);
    REQUIRE(prog.size() == 11);
    CHECK(prog[2].op == OpCode::Add);
    CHECK(prog[5].op == OpCode::Concat);
    CHECK(prog[7].op == OpCode::PrintInt);
    CHECK(prog[9].op == OpCode::PrintStr);
    CHECK(prog[10].op == OpCode::Halt);
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
    auto prog = parser.parse(in);
    REQUIRE(prog.size() == 8);
    CHECK(prog[1].op == OpCode::JumpIfZero);
    CHECK(std::get<int>(prog[1].operand) == 4);
    CHECK(prog[5].op == OpCode::Jump);
    CHECK(std::get<int>(prog[5].operand) == 7);
}

TEST_CASE("parser label only then opcode") {
    Parser parser;
    std::istringstream in(
        "entry:\n"
        "PUSH_INT 1\n"
        "HALT\n");
    auto prog = parser.parse(in);
    REQUIRE(prog.size() == 2);
    CHECK(prog[0].op == OpCode::PushInt);
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

TEST_CASE("parser parseFile missing file") {
    Parser parser;
    CHECK_THROWS_AS(parser.parseFile("/nonexistent/pick_system/no_such_file.tbc"), std::runtime_error);
}
