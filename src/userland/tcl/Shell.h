//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_TCL_SHELL_H
#define PICK_SYSTEM_TCL_SHELL_H

#pragma once

#include "BasicShell.h"
#include "ShellSession.h"

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace PickShell {
    class Shell {
    public:
        explicit Shell(PickVM::Runtime &runtime);

        // Root for LIST-PROGRAMS and relative RUN paths (default "programs").
        void setProgramsRoot(std::filesystem::path root);

        const std::filesystem::path &programsRoot() const { return session_.programsRoot(); }

        // Root for CREATE-FILE / DELETE-FILE / LIST-FILES / READ / WRITE (default "filesystem").
        void setFileSystemRoot(std::filesystem::path root);

        const std::filesystem::path &fileSystemRoot() const { return session_.fileSystemRoot(); }

        // One line of input; all command output goes to out. For interactive use, pass std::cout.
        void handleLine(const std::string &line, std::ostream &out, bool &quit);

        void run(); // REPL: stdin, prompts on std::cout, command output on std::cout

    private:
        using Tokens = std::vector<std::string>;
        using CommandFn = std::function<void(const Tokens &, std::ostream &, bool &)>;
        using CommandTable = std::unordered_map<std::string, CommandFn>;

        ShellSession session_;
        BasicShell basicShell_;
        CommandTable tclCommands_;

        static std::vector<std::string> tokenize(const std::string &line);

        void dispatch(const std::vector<std::string> &tokens, bool &quit, std::ostream &out);

        [[nodiscard]] std::string prompt() const;

        void registerCommands();

        void registerTclCommands();

        void handleTclCommand(const Tokens &tokens, bool &quit, std::ostream &out);

        void executeCompiledBasicProgram(const std::vector<PickVM::Instruction> &program, std::ostream &out);

        void cmdEcho(const std::vector<std::string> &tokens, std::ostream &out);

        [[nodiscard]] std::string expandEchoToken(const std::string &token) const;

        void cmdRun(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdHelp(std::ostream &out);

        void cmdVersion(std::ostream &out);

        void cmdDumpStack(std::ostream &out);

        void cmdListPrograms(std::ostream &out);

        void cmdCreateFile(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdDeleteFile(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdListFiles(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdList(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdRead(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdWrite(const std::vector<std::string> &tokens, std::ostream &out);

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
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_SHELL_H
