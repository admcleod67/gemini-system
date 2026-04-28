#include "AssemblerShell.h"

#include <exception>
#include <utility>

namespace PickShell {
    AssemblerShell::AssemblerShell(VmDebugService &debugService)
        : debugService_(debugService) {
        registerCommands();
    }

    void AssemblerShell::setRunCommandFn(RunCommandFn runCommandFn) {
        runCommandFn_ = std::move(runCommandFn);
    }

    void AssemblerShell::setDumpStackFn(DumpStackFn dumpStackFn) {
        dumpStackFn_ = std::move(dumpStackFn);
    }

    void AssemblerShell::enter(std::ostream &out) {
        if (mode_ == Mode::Assembler) {
            out << "Already in ASM mode\n";
            return;
        }
        mode_ = Mode::Assembler;
    }

    bool AssemblerShell::isActive() const {
        return mode_ == Mode::Assembler;
    }

    std::string AssemblerShell::prompt() const {
        return "ASM> ";
    }

    void AssemblerShell::handleCommand(const Tokens &tokens, std::ostream &out, bool &leaveAssemblerMode) {
        leaveAssemblerMode = false;
        if (tokens.empty() || mode_ != Mode::Assembler) {
            return;
        }

        const auto it = commands_.find(tokens[0]);
        if (it == commands_.end()) {
            out << "Unknown command: " << tokens[0] << "\n";
            return;
        }
        it->second(tokens, out, leaveAssemblerMode);
    }

    void AssemblerShell::registerCommands() {
        commands_["HELP"] = [this](const Tokens &, std::ostream &out, bool &) { cmdHelp(out); };
        commands_["QUIT"] = [this](const Tokens &, std::ostream &, bool &leave) {
            mode_ = Mode::Inactive;
            leave = true;
        };
        commands_["END"] = [this](const Tokens &, std::ostream &out, bool &) {
            if (!debugService_.hasProgramLoaded()) {
                out << "No program loaded\n";
                return;
            }
            debugService_.endProgramContext();
        };
        commands_["RUN"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (!runCommandFn_) {
                out << "Error: ASM runtime not configured\n";
                return;
            }
            runCommandFn_(tokens, out);
        };
        commands_["CONT"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 1) {
                out << "CONT takes no arguments\n";
                return;
            }
            if (!runCommandFn_) {
                out << "Error: ASM runtime not configured\n";
                return;
            }
            runCommandFn_({"RUN"}, out);
        };
        commands_["TRACE"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 2 || (tokens[1] != "ON" && tokens[1] != "OFF")) {
                out << "TRACE requires ON or OFF\n";
                return;
            }
            debugService_.setTrace(tokens[1] == "ON");
        };
        commands_["STEP"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 1) {
                out << "STEP takes no arguments\n";
                return;
            }
            const VmDebugService::StepResult result = debugService_.stepVm(out);
            if (result.outcome == VmDebugService::StepOutcome::NoProgramLoaded) {
                out << "No program loaded\n";
            } else if (result.outcome == VmDebugService::StepOutcome::Halted) {
                out << "Program finished\n";
            } else if (result.outcome == VmDebugService::StepOutcome::RuntimeError) {
                out << "Runtime error: " << result.runtimeError << '\n';
            }
        };
        commands_["BREAKPOINT"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 2) {
                out << "BREAKPOINT requires an index\n";
                return;
            }
            try {
                debugService_.addBreakpoint(static_cast<std::size_t>(std::stoull(tokens[1])));
            } catch (const std::exception &) {
                out << "BREAKPOINT requires a non-negative integer index\n";
            }
        };
        commands_["BREAKPOINTS"] = [this](const Tokens &, std::ostream &out, bool &) {
            const std::vector<std::size_t> breakpoints = debugService_.sortedBreakpoints();
            if (breakpoints.empty()) {
                out << "No breakpoints\n";
                return;
            }
            out << "Breakpoints:";
            for (const std::size_t breakpoint: breakpoints) {
                out << ' ' << breakpoint;
            }
            out << '\n';
        };
        commands_["CLEAR-BREAKPOINT"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 2) {
                out << "CLEAR-BREAKPOINT requires an index\n";
                return;
            }
            try {
                const std::size_t index = static_cast<std::size_t>(std::stoull(tokens[1]));
                if (!debugService_.removeBreakpoint(index)) {
                    out << "No such breakpoint\n";
                }
            } catch (const std::exception &) {
                out << "CLEAR-BREAKPOINT requires a non-negative integer index\n";
            }
        };
        commands_["CLEAR-BREAKPOINTS"] = [this](const Tokens &, std::ostream &, bool &) {
            debugService_.clearBreakpoints();
        };
        commands_["DUMP-PROGRAM"] = [this](const Tokens &, std::ostream &out, bool &) {
            debugService_.dumpProgram(out);
        };
        commands_["DUMP-LABELS"] = [this](const Tokens &, std::ostream &out, bool &) {
            debugService_.dumpLabels(out);
        };
        commands_["DUMP-STACK"] = [this](const Tokens &, std::ostream &out, bool &) {
            if (!dumpStackFn_) {
                out << "Error: ASM runtime not configured\n";
                return;
            }
            dumpStackFn_(out);
        };
    }

    void AssemblerShell::cmdHelp(std::ostream &out) const {
        out << "ASM commands:\n";
        out << "  RUN [name]\n";
        out << "  CONT\n";
        out << "  STEP\n";
        out << "  TRACE ON|OFF\n";
        out << "  BREAKPOINT <n>\n";
        out << "  BREAKPOINTS\n";
        out << "  CLEAR-BREAKPOINT <n>\n";
        out << "  CLEAR-BREAKPOINTS\n";
        out << "  DUMP-STACK\n";
        out << "  DUMP-PROGRAM\n";
        out << "  DUMP-LABELS\n";
        out << "  END            exit VM debugger context\n";
        out << "  QUIT\n";
    }
} // namespace PickShell
