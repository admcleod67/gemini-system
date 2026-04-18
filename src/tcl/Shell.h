//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_TCL_SHELL_H
#define PICK_SYSTEM_TCL_SHELL_H

#pragma once

#include "../core/vm/Runtime.h"

#include <filesystem>
#include <iosfwd>
#include <string>
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
        std::filesystem::path programsRoot_{"programs"};

        static std::vector<std::string> tokenize(const std::string &line);

        void dispatch(const std::vector<std::string> &tokens, bool &quit, std::ostream &out);

        void cmdEcho(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdRun(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdHelp(std::ostream &out);

        void cmdVersion(std::ostream &out);

        void cmdDumpStack(std::ostream &out);

        void cmdListPrograms(std::ostream &out);
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_SHELL_H
