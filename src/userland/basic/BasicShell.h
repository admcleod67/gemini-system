//
// BASIC shell/interpreter (BASIC> and ED> modes).
//

#ifndef PICK_SYSTEM_BASIC_BASICSHELL_H
#define PICK_SYSTEM_BASIC_BASICSHELL_H

#pragma once

#include "BasicCompiler.h"
#include "BasicProgram.h"

#include <filesystem>
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
        using ExecuteProgramFn = std::function<void(const std::vector<PickVM::Instruction> &, std::ostream &)>;

        BasicShell();

        void setProgramsRoot(std::filesystem::path programsRoot);

        void setExecuteProgramFn(ExecuteProgramFn executeProgramFn);

        void enter(std::optional<std::string> programName, std::ostream &out);

        [[nodiscard]] bool isActive() const;

        [[nodiscard]] std::string prompt() const;

        void handleCommand(const Tokens &tokens, std::ostream &out, bool &leaveBasicMode);

    private:
        enum class Mode {
            Inactive,
            Basic,
            Editor,
        };

        struct EditorState {
            int lineNumber{0};
            std::string text;
        };

        std::filesystem::path programsRoot_{"programs"};
        Mode mode_{Mode::Inactive};
        BasicCompiler compiler_;
        BasicProgram program_;
        std::optional<EditorState> editorState_;
        ExecuteProgramFn executeProgramFn_;
        CommandTable basicCommands_;
        CommandTable editorCommands_;

        static bool parseLineNumber(const std::string &token, int &lineNumber);

        static std::string joinTokens(const Tokens &tokens, std::size_t startIndex);

        static bool parseDeleteRange(const std::string &token, int &startLine, int &endLine);

        static bool applyEditorSubstitute(std::string &line, const std::string &command);

        void registerCommands();

        void registerBasicCommands();

        void registerEditorCommands();

        void handleBasicCommand(const Tokens &tokens, std::ostream &out, bool &leaveBasicMode);

        void handleEditorCommand(const Tokens &tokens, std::ostream &out);

        void enterEditorMode(int lineNumber, std::ostream &out);

        void loadProgramIfPresent(std::ostream &out);

        void saveProgram(const std::string &name, std::ostream &out);

        void printCompileSuccess(const BasicCompileResult &compile, std::ostream &out);

        void printCompileFailure(const BasicCompileResult &compile, std::ostream &out);
    };
} // namespace PickShell

#endif // PICK_SYSTEM_BASIC_BASICSHELL_H
