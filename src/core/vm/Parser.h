//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_PARSER_H
#define PICK_SYSTEM_PARSER_H

#include "Runtime.h"

#include <istream>
#include <string>
#include <vector>

#include <unordered_map>

namespace PickVM {
    struct ParsedLine {
        std::string label; // empty if none
        std::string opcode; // e.g. "PUSH_INT"
        std::string operand; // raw operand text
        int sourceLine = 0; // physical source line (1-based) when emitted
    };

    class Parser {
    public:
        // Load text bytecode from a stream (newline-separated), resolve labels
        std::vector<Instruction> parse(std::istream &in);

        // Load a text bytecode file, resolve labels, and return instructions
        std::vector<Instruction> parseFile(const std::string &filename);

    private:
        static ParsedLine parseLine(const std::string &line);

        static void validateJumpTargets(const std::vector<Instruction> &program);

        static std::string trim(const std::string &s);

        std::unordered_map<std::string, int> labelMap_;
        std::vector<ParsedLine> intermediateLines_;
    };
} // namespace PickVM

#endif //PICK_SYSTEM_PARSER_H
