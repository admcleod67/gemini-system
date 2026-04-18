//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_INSTRUCTION_PRINT_H
#define PICK_SYSTEM_INSTRUCTION_PRINT_H

#include "Runtime.h"

#include <cstddef>
#include <string>

namespace PickVM {
    struct LoadedBytecode;

    const char *opCodeName(OpCode op);

    // One line: "{ip}: MNEMONIC ..." matching trace / DUMP-PROGRAM. Optional source .tbc line suffix.
    std::string formatInstructionLine(std::size_t ip, const Instruction &instr, const LoadedBytecode *loaded);
} // namespace PickVM

#endif // PICK_SYSTEM_INSTRUCTION_PRINT_H
