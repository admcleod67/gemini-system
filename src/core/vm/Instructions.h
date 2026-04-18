//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_INSTRUCTIONS_H
#define PICK_SYSTEM_INSTRUCTIONS_H

#include "VM.h"

#include <istream>
#include <string>
#include <vector>
#include <unordered_map>

namespace PickVM {

    struct ParsedLine {
        std::string label;       // empty if none
        std::string opcode;      // e.g. "PUSH_INT"
        std::string operand;     // raw operand text
        int sourceLine = 0;      // physical source line (1-based) when emitted
    };

    // Parse a single line of text bytecode into label/op/operand
    ParsedLine parseLine(const std::string& line);

    // Load text bytecode from a stream (newline-separated), resolve labels
    std::vector<Instruction> loadTextBytecodeStream(std::istream& in);

    // Load a text bytecode file, resolve labels, and return instructions
    std::vector<Instruction> loadTextBytecode(const std::string& filename);

} // namespace PickVM



#endif //PICK_SYSTEM_INSTRUCTIONS_H
