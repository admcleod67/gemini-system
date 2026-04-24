#ifndef PICK_SYSTEM_BASIC_BASICBYTECODEEMITTER_H
#define PICK_SYSTEM_BASIC_BASICBYTECODEEMITTER_H

#pragma once

#include "BasicCompiler.h"
#include "BasicNormalizedIr.h"

#include <vector>

namespace PickShell {
    struct BasicBytecodeEmissionResult {
        bool success{false};
        std::vector<PickVM::Instruction> program;
        std::vector<BasicCompileError> errors;
    };

    class BasicBytecodeEmitter {
    public:
        [[nodiscard]] static BasicBytecodeEmissionResult emit(const BasicIr::NormalizedProgram &program);
    };
} // namespace PickShell

#endif // PICK_SYSTEM_BASIC_BASICBYTECODEEMITTER_H
