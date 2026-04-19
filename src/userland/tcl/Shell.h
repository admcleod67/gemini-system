//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_TCL_SHELL_H
#define PICK_SYSTEM_TCL_SHELL_H

#pragma once

#include <pickvm/core.hpp>

#include "TclEnvironment.h"

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace PickShell {
    class Shell {
    public:
        explicit Shell(PickVM::Runtime &runtime);

        // Root for LIST-PROGRAMS and relative RUN paths (default "programs").
        void setProgramsRoot(std::filesystem::path root);

        const std::filesystem::path &programsRoot() const { return programsRoot_; }

        // One line of input; all command output goes to out. For interactive use, pass std::cout.
        void handleLine(const std::string &line, std::ostream &out, bool &quit);

        void run(); // REPL: stdin, prompts on std::cout, command output on std::cout

    private:
        PickVM::Runtime &runtime_;
        TclEnvironment env_;
        std::filesystem::path programsRoot_{"programs"};

        static std::vector<std::string> tokenize(const std::string &line);

        void dispatch(const std::vector<std::string> &tokens, bool &quit, std::ostream &out);

        void cmdEcho(const std::vector<std::string> &tokens, std::ostream &out);

        [[nodiscard]] std::string expandEchoToken(const std::string &token) const;

        void cmdRun(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdHelp(std::ostream &out);

        void cmdVersion(std::ostream &out);

        void cmdDumpStack(std::ostream &out);

        void cmdListPrograms(std::ostream &out);

        void cmdTrace(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdStep(std::ostream &out);

        void cmdBreakpoint(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdBreakpoints(std::ostream &out);

        void cmdClearBreakpoint(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdClearBreakpoints(std::ostream &out);

        void cmdDumpProgram(std::ostream &out);

        void cmdDumpLabels(std::ostream &out);

        void cmdSet(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdGet(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdListVars(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdUnset(const std::vector<std::string> &tokens, std::ostream &out);

        void executeVmLoop(std::ostream &out);

        void pruneBreakpointsForProgram(std::size_t programSize, std::ostream &out);

        bool programImageLoaded() const;

        std::optional<PickVM::LoadedBytecode> lastLoaded_;
        bool trace_{false};
        std::unordered_set<std::size_t> breakpoints_;
        bool suspended_{false};
        std::optional<std::size_t> resumePastBreakpointIp_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_SHELL_H
