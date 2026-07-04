//
// One running Gemini System session: owns VM runtime and Tcl shell front-end.
//

#ifndef PICK_SYSTEM_TCL_GEMINI_SESSION_H
#define PICK_SYSTEM_TCL_GEMINI_SESSION_H

#pragma once

#include "Shell.h"

namespace PickShell {
    class GeminiSession {
    public:
        GeminiSession();

        [[nodiscard]] PickVM::Runtime &runtime() noexcept { return runtime_; }
        [[nodiscard]] const PickVM::Runtime &runtime() const noexcept { return runtime_; }

        [[nodiscard]] Shell &shell() noexcept { return shell_; }
        [[nodiscard]] const Shell &shell() const noexcept { return shell_; }

    private:
        PickVM::Runtime runtime_;
        Shell shell_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_GEMINI_SESSION_H
