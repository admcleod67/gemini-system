//
// BASIC shell/interpreter (BASIC>).
//

#ifndef PICK_SYSTEM_BASIC_BASICSHELL_H
#define PICK_SYSTEM_BASIC_BASICSHELL_H

#pragma once

#include "BasicCompiler.h"
#include "BasicProgram.h"

#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace PickShell {
    class BasicShell {
    public:
        using Tokens = std::vector<std::string>;
        using CommandFn = std::function<void(const Tokens &, std::ostream &, bool &)>;
        using CommandTable = std::unordered_map<std::string, CommandFn>;
        using ExecuteProgramFn = std::function<void(const std::vector<PickVM::Instruction> &,
                                                    const std::vector<int> &,
                                                    const BasicProgram &,
                                                    std::ostream &)>;
        struct ProgramLocation {
            std::string fileName;
            std::string recordKey;
        };
        using ResolveProgramLocationFn = std::function<ProgramLocation(const std::string &)>;
        using ReadRecordFn = std::function<std::optional<std::string>(const ProgramLocation &, bool objectRecord)>;
        using WriteRecordFn = std::function<bool(const ProgramLocation &, bool objectRecord, const std::string &, std::string &)>;
        using RunLineRecordEditorFn =
            std::function<void(const ProgramLocation &, const std::optional<int> highlightPhysicalLine, std::ostream &out)>;

        BasicShell();

        void setResolveProgramLocationFn(ResolveProgramLocationFn resolveProgramLocationFn);

        void setReadRecordFn(ReadRecordFn readRecordFn);

        void setWriteRecordFn(WriteRecordFn writeRecordFn);

        void setExecuteProgramFn(ExecuteProgramFn executeProgramFn);

        void setRunLineRecordEditorFn(RunLineRecordEditorFn runLineRecordEditorFn);

        void enter(std::optional<std::string> programName, std::ostream &out);

        [[nodiscard]] bool isActive() const;

        [[nodiscard]] std::string prompt() const;

        void handleCommand(const Tokens &tokens, std::ostream &out, bool &leaveBasicMode);

    private:
        enum class Mode {
            Inactive,
            Basic,
        };

        Mode mode_{Mode::Inactive};
        BasicCompiler compiler_;
        BasicProgram program_;
        ExecuteProgramFn executeProgramFn_;
        ResolveProgramLocationFn resolveProgramLocationFn_;
        ReadRecordFn readRecordFn_;
        WriteRecordFn writeRecordFn_;
        RunLineRecordEditorFn runLineRecordEditorFn_;
        CommandTable basicCommands_;

        static bool parseLineNumber(const std::string &token, int &lineNumber);

        static std::string joinTokens(const Tokens &tokens, std::size_t startIndex);

        static bool parseDeleteRange(const std::string &token, int &startLine, int &endLine);

        [[nodiscard]] std::optional<int> physicalRowForBasicLine(int basicLineNumber) const;

        bool flushProgramSourceToDisk(std::ostream &out);

        void registerCommands();

        void registerBasicCommands();

        void handleBasicCommand(const Tokens &tokens, std::ostream &out, bool &leaveBasicMode);

        void loadProgramIfPresent(std::ostream &out);

        void saveProgram(const std::string &name, std::ostream &out);

        bool saveObjectCode(const std::string &name, const BasicCompileResult &compile, std::ostream &out);

        void printCompileSuccess(const BasicCompileResult &compile, std::ostream &out);

        void printCompileFailure(const BasicCompileResult &compile, std::ostream &out);
    };
} // namespace PickShell

#endif // PICK_SYSTEM_BASIC_BASICSHELL_H
