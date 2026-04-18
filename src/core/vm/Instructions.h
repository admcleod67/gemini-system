//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_INSTRUCTIONS_H
#define PICK_SYSTEM_INSTRUCTIONS_H

#include "VM.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace PickVM {

    struct ParsedLine {
        std::string label;       // empty if none
        std::string opcode;      // e.g. "PUSH_INT"
        std::string operand;     // raw operand text
    };

    // Parse a single line of text bytecode into label/op/operand
    ParsedLine parseLine(const std::string& line);

    // Load a text bytecode file, resolve labels, and return instructions
    std::vector<Instruction> loadTextBytecode(const std::string& filename);

} // namespace PickVM



#endif //PICK_SYSTEM_INSTRUCTIONS_H
