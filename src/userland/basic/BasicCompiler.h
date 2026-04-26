//
// BASIC compiler placeholder for a future implementation round.
//

#ifndef PICK_SYSTEM_BASIC_BASICCOMPILER_H
#define PICK_SYSTEM_BASIC_BASICCOMPILER_H

#pragma once

#include "BasicProgram.h"
#include "Runtime.h"

#include <cstddef>
#include <string>
#include <vector>

namespace PickShell {
    struct BasicCompileError {
        int line{0};
        std::string message;
    };

    struct BasicCompileResult {
        bool success{false};
        std::vector<PickVM::Instruction> program;
        std::vector<int> sourceLinePerInstr; // parallel to program; 0 means "no source line"
        std::size_t instructionCount{0};
        std::size_t labelCount{0};
        std::vector<BasicCompileError> errors;
    };

    class BasicCompiler {
    public:
        [[nodiscard]] BasicCompileResult compile(const BasicProgram &program) const;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_BASIC_BASICCOMPILER_H
