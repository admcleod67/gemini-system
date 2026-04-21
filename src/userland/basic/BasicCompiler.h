//
// BASIC compiler placeholder for a future implementation round.
//

#ifndef PICK_SYSTEM_BASIC_BASICCOMPILER_H
#define PICK_SYSTEM_BASIC_BASICCOMPILER_H

#pragma once

#include "BasicProgram.h"

#include <string>

namespace PickShell {
    struct BasicCompileResult {
        bool supported{false};
        std::string message{"BASIC compiler is not implemented yet"};
    };

    class BasicCompiler {
    public:
        [[nodiscard]] BasicCompileResult compile(const BasicProgram &program) const;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_BASIC_BASICCOMPILER_H
