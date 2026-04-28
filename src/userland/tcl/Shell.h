//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_TCL_SHELL_H
#define PICK_SYSTEM_TCL_SHELL_H

#pragma once

#include "AssemblerShell.h"
#include "BasicShell.h"
#include "ProcInterpreter.h"
#include "ShellSession.h"

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

        // Optional VM input source for instructions like INPUT_INT. nullptr selects std::cin.
        void setInputStream(std::istream *in);

        void run(); // REPL: stdin, prompts on std::cout, command output on std::cout

    private:
        using Tokens = std::vector<std::string>;
        using CommandFn = std::function<void(const Tokens &, std::ostream &, bool &)>;
        using CommandTable = std::unordered_map<std::string, CommandFn>;

        ShellSession session_;
        VmDebugService vmDebugService_;
        AssemblerShell assemblerShell_;
        BasicShell basicShell_;
        ProcInterpreter procInterpreter_;
        CommandTable tclCommands_;
        std::istream *inputStream_{nullptr};

        static std::vector<std::string> tokenize(const std::string &line);

        void dispatch(const std::vector<std::string> &tokens, bool &quit, std::ostream &out);

        [[nodiscard]] std::string prompt() const;

        void registerCommands();

        void registerTclCommands();

        void handleTclCommand(const Tokens &tokens, bool &quit, std::ostream &out);

        void executeCompiledBasicProgram(const std::vector<PickVM::Instruction> &program,
                                         const std::vector<int> &sourceLinePerInstr,
                                         const BasicProgram &sourceProgram,
                                         std::ostream &out);

        bool runBasicUntilStop(const std::vector<PickVM::Instruction> &program,
                               const std::vector<int> &sourceLinePerInstr,
                               std::ostream &out,
                               bool stopAtNextBasicLine);
        bool handleBasicDebuggerCommand(const std::vector<std::string> &tokens,
                                        const std::vector<PickVM::Instruction> &program,
                                        const std::vector<int> &sourceLinePerInstr,
                                        const BasicProgram &sourceProgram,
                                        std::ostream &out);
        static std::optional<int> parsePositiveInt(const std::string &token);
        static std::vector<std::string> tokenizeDebuggerLine(const std::string &line);
        static std::set<int> sourceLinesFromMetadata(const std::vector<int> &sourceLinePerInstr);
        static bool parseBasicListRange(const std::vector<std::string> &tokens, int &startLine, int &endLine);
        static void listBasicSourceRange(const BasicProgram &program, int startLine, int endLine, std::ostream &out);
        static std::optional<std::size_t> firstInstructionForSourceLine(const std::vector<int> &sourceLinePerInstr,
                                                                        int sourceLine);

        void cmdEcho(const std::vector<std::string> &tokens, std::ostream &out);

        [[nodiscard]] std::string expandEchoToken(const std::string &token) const;

        void cmdRun(const std::vector<std::string> &tokens, std::ostream &out);
        void cmdProc(const std::vector<std::string> &tokens, std::ostream &out);
        void executeProcTclCommand(const std::string &line, std::ostream &out);
        bool resolveRunProgramPaths(const std::string &programName,
                                    std::filesystem::path &sourcePath,
                                    std::filesystem::path &bytecodePath,
                                    std::ostream &out) const;
        bool ensureBytecodeExistsForRun(const std::string &programName,
                                        const std::filesystem::path &sourcePath,
                                        const std::filesystem::path &bytecodePath,
                                        std::ostream &out);
        static bool loadBasicSourceProgram(const std::filesystem::path &sourcePath,
                                           const std::string &programName,
                                           BasicProgram &program);
        static bool writeCompiledBytecode(const std::filesystem::path &bytecodePath,
                                          const BasicCompileResult &compile,
                                          std::ostream &out);

        void cmdHelp(std::ostream &out);

        void cmdVersion(std::ostream &out);
        void cmdWho(std::ostream &out);

        void cmdDumpStack(std::ostream &out);

        void cmdListPrograms(std::ostream &out);

        void cmdCreateFile(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdDeleteFile(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdListFiles(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdList(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdRead(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdWrite(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdSet(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdGet(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdListVars(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdUnset(const std::vector<std::string> &tokens, std::ostream &out);

        std::unordered_set<int> basicBreakpoints_;
        std::optional<int> basicResumePastLine_;
        bool basicTrace_{false};
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_SHELL_H
