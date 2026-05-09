//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_TCL_SHELL_H
#define PICK_SYSTEM_TCL_SHELL_H

#pragma once

#include "AssemblerShell.h"
#include "BasicShell.h"
#include "EnglishService.h"
#include "ProcInterpreter.h"
#include "ShellSession.h"
#include "UserSession.h"

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
    enum class ShellRunResult {
        ExitProcess,
        EndSession,
    };

    class Shell {
    public:
        explicit Shell(PickVM::Runtime &runtime);

        // Legacy host root setting retained for compatibility.
        void setProgramsRoot(std::filesystem::path root);

        const std::filesystem::path &programsRoot() const { return session_.programsRoot(); }

        // Root for CREATE-FILE / DELETE-FILE / LIST-FILES / READ / WRITE (default "filesystem").
        void setFileSystemRoot(std::filesystem::path root);

        const std::filesystem::path &fileSystemRoot() const { return session_.fileSystemRoot(); }

        /// Parent directory of ACCOUNTS.json / USERS.json. When set, the host (`main`) runs core login before the Tcl REPL.
        void setGeminiCatalogRoot(std::optional<std::filesystem::path> root);

        [[nodiscard]] const std::optional<std::filesystem::path> &geminiCatalogRoot() const { return session_.geminiCatalogRoot(); }

        /// Apply a successful core login (filesystem root, identity, Tcl `@*` variables).
        void attachUserSession(const PickCore::UserSession &session);

        // One line of input; all command output goes to out. For interactive use, pass std::cout.
        void handleLine(const std::string &line, std::ostream &out, bool &quit);

        // Optional VM input source for instructions like INPUT_INT. nullptr selects std::cin.
        void setInputStream(std::istream *in);

        /// Tcl REPL only (no login). `LOGOFF` yields `EndSession`; `QUIT` / EOF yields `ExitProcess`.
        [[nodiscard]] ShellRunResult runTclRepl();

    private:
        using Tokens = std::vector<std::string>;
        using CommandFn = std::function<void(const Tokens &, std::ostream &, bool &)>;
        using CommandTable = std::unordered_map<std::string, CommandFn>;

        ShellSession session_;
        VmDebugService vmDebugService_;
        AssemblerShell assemblerShell_;
        BasicShell basicShell_;
        ProcInterpreter procInterpreter_;
        PickCore::English::EnglishService englishService_;
        CommandTable tclCommands_;
        std::istream *inputStream_{nullptr};
        bool sessionEndRequested_{false};

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
        bool procSelect(const std::string &fileName, std::string &error);
        std::optional<std::string> procReadNext(std::string &error);
        void resetProcReadNextCursor();
        static std::string programObjectRecordKey(const std::string &recordKey);
        std::optional<std::string> readProgramRecord(const BasicShell::ProgramLocation &location, bool objectRecord);
        bool writeProgramRecord(const BasicShell::ProgramLocation &location,
                                bool objectRecord,
                                const std::string &payload,
                                std::string &error);
        bool ensureProgramObjectExistsForRun(const std::string &programName, std::ostream &out);

        void cmdHelp(std::ostream &out);

        void cmdVersion(std::ostream &out);
        void cmdWho(std::ostream &out);

        void cmdDumpStack(std::ostream &out);

        void cmdListPrograms(std::ostream &out);

        void cmdCreateFile(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdDeleteFile(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdListFiles(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdList(const std::vector<std::string> &tokens, std::ostream &out);
        void cmdCount(const std::vector<std::string> &tokens, std::ostream &out);
        void cmdSelect(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdSort(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdListList(const std::vector<std::string> &tokens, std::ostream &out);
        void cmdClearList(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdResolveField(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdDefineField(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdRead(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdWrite(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdEdit(const std::vector<std::string> &tokens, std::ostream &out);

        void runLineRecordEditorForLocation(const std::string &fileName,
                                            const std::string &recordKey,
                                            const std::optional<int> &highlightPhysicalLine,
                                            std::ostream &out);

        void cmdSet(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdGet(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdListVars(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdUnset(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdLogto(const std::vector<std::string> &tokens, std::ostream &out);

        void cmdLogoff(const std::vector<std::string> &tokens, std::ostream &out);

        std::unordered_set<int> basicBreakpoints_;
        std::optional<int> basicResumePastLine_;
        bool basicTrace_{false};
        std::size_t procReadNextIndex_{0};
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_SHELL_H
