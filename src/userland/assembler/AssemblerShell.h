#ifndef PICK_SYSTEM_ASSEMBLER_ASSEMBLERSHELL_H
#define PICK_SYSTEM_ASSEMBLER_ASSEMBLERSHELL_H

#pragma once

#include "VmDebugService.h"

#include <functional>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

namespace PickShell {
    class AssemblerShell {
    public:
        using Tokens = std::vector<std::string>;
        using CommandFn = std::function<void(const Tokens &, std::ostream &, bool &)>;
        using CommandTable = std::unordered_map<std::string, CommandFn>;
        using RunCommandFn = std::function<void(const Tokens &, std::ostream &)>;
        using DumpStackFn = std::function<void(std::ostream &)>;

        explicit AssemblerShell(VmDebugService &debugService);

        void setRunCommandFn(RunCommandFn runCommandFn);
        void setDumpStackFn(DumpStackFn dumpStackFn);

        void enter(std::ostream &out);
        [[nodiscard]] bool isActive() const;
        [[nodiscard]] std::string prompt() const;
        void handleCommand(const Tokens &tokens, std::ostream &out, bool &leaveAssemblerMode);

    private:
        enum class Mode {
            Inactive,
            Assembler,
        };

        Mode mode_{Mode::Inactive};
        VmDebugService &debugService_;
        RunCommandFn runCommandFn_;
        DumpStackFn dumpStackFn_;
        CommandTable commands_;

        void registerCommands();
        void cmdHelp(std::ostream &out) const;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_ASSEMBLER_ASSEMBLERSHELL_H
