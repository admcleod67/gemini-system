#ifndef PICK_SYSTEM_ASSEMBLER_VMDEBUGSERVICE_H
#define PICK_SYSTEM_ASSEMBLER_VMDEBUGSERVICE_H

#pragma once

#include "ShellSession.h"

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace PickShell {
    class VmDebugService {
    public:
        enum class StepOutcome {
            NoProgramLoaded,
            Advanced,
            Halted,
            Interrupted,
            RuntimeError,
        };

        struct StepResult {
            StepOutcome outcome{StepOutcome::Advanced};
            std::string runtimeError;
        };

        explicit VmDebugService(ShellSession &session)
            : session_(session) {}

        [[nodiscard]] bool hasProgramLoaded() const;
        void setTrace(bool traceEnabled);
        [[nodiscard]] bool trace() const;

        void addBreakpoint(std::size_t index);
        [[nodiscard]] bool removeBreakpoint(std::size_t index);
        void clearBreakpoints();
        [[nodiscard]] std::vector<std::size_t> sortedBreakpoints() const;

        void endProgramContext();
        void dumpProgram(std::ostream &out) const;
        void dumpLabels(std::ostream &out) const;
        StepResult stepVm(std::ostream &out);

        StepResult stepRuntime(bool traceEnabled,
                               std::ostream &out,
                               const PickVM::LoadedBytecode *loadedView = nullptr) const;

    private:
        ShellSession &session_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_ASSEMBLER_VMDEBUGSERVICE_H
