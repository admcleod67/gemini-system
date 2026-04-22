#include "BasicShell.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace PickShell {
    BasicShell::BasicShell() {
        registerCommands();
    }

    void BasicShell::setProgramsRoot(std::filesystem::path programsRoot) {
        programsRoot_ = std::move(programsRoot);
    }

    void BasicShell::enter(std::optional<std::string> programName, std::ostream &out) {
        mode_ = Mode::Basic;
        editorState_.reset();
        program_.setName(std::move(programName));
        program_.clearLines();
        loadProgramIfPresent(out);
    }

    bool BasicShell::isActive() const {
        return mode_ != Mode::Inactive;
    }

    std::string BasicShell::prompt() const {
        if (mode_ == Mode::Editor) {
            return "ED> ";
        }
        return "BASIC> ";
    }

    void BasicShell::handleCommand(const Tokens &tokens, std::ostream &out, bool &leaveBasicMode) {
        if (!isActive()) {
            return;
        }
        leaveBasicMode = false;
        if (mode_ == Mode::Editor) {
            handleEditorCommand(tokens, out);
            return;
        }
        handleBasicCommand(tokens, out, leaveBasicMode);
    }

    bool BasicShell::parseLineNumber(const std::string &token, int &lineNumber) {
        if (token.empty()) {
            return false;
        }
        for (const char c: token) {
            if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
                return false;
            }
        }
        try {
            lineNumber = std::stoi(token);
            return lineNumber > 0;
        } catch (const std::exception &) {
            return false;
        }
    }

    std::string BasicShell::joinTokens(const Tokens &tokens, const std::size_t startIndex) {
        std::string out;
        for (std::size_t i = startIndex; i < tokens.size(); ++i) {
            if (i > startIndex) {
                out += ' ';
            }
            out += tokens[i];
        }
        return out;
    }

    bool BasicShell::parseDeleteRange(const std::string &token, int &startLine, int &endLine) {
        const std::size_t dashPos = token.find('-');
        if (dashPos == std::string::npos) {
            if (!parseLineNumber(token, startLine)) {
                return false;
            }
            endLine = startLine;
            return true;
        }

        const std::string startText = token.substr(0, dashPos);
        const std::string endText = token.substr(dashPos + 1);
        if (!parseLineNumber(startText, startLine) || !parseLineNumber(endText, endLine)) {
            return false;
        }
        return startLine <= endLine;
    }

    bool BasicShell::applyEditorSubstitute(std::string &line, const std::string &command) {
        if (command.size() < 4 || command[0] != 'C') {
            return false;
        }
        const char delimiter = command[1];
        const std::size_t firstDelimiter = command.find(delimiter, 2);
        if (firstDelimiter == std::string::npos) {
            return false;
        }
        const std::size_t secondDelimiter = command.find(delimiter, firstDelimiter + 1);
        if (secondDelimiter == std::string::npos || secondDelimiter != command.size() - 1) {
            return false;
        }

        const std::string from = command.substr(2, firstDelimiter - 2);
        const std::string to = command.substr(firstDelimiter + 1, secondDelimiter - firstDelimiter - 1);
        if (from.empty()) {
            return false;
        }

        const std::size_t pos = line.find(from);
        if (pos == std::string::npos) {
            return true;
        }
        line.replace(pos, from.size(), to);
        return true;
    }

    void BasicShell::registerCommands() {
        registerBasicCommands();
        registerEditorCommands();
    }

    void BasicShell::registerBasicCommands() {
        basicCommands_["LIST"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() > 1) {
                out << "LIST takes no arguments\n";
                return;
            }
            program_.list(out);
        };
        basicCommands_["EDIT"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 2) {
                out << "EDIT requires a line number\n";
                return;
            }
            int lineNumber = 0;
            if (!parseLineNumber(tokens[1], lineNumber)) {
                out << "EDIT requires a line number\n";
                return;
            }
            enterEditorMode(lineNumber, out);
        };
        basicCommands_["DELETE"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() != 2) {
                out << "DELETE requires a line number or range\n";
                return;
            }
            int startLine = 0;
            int endLine = 0;
            if (!parseDeleteRange(tokens[1], startLine, endLine)) {
                out << "DELETE requires a line number or range\n";
                return;
            }
            (void) program_.deleteRange(startLine, endLine);
        };
        basicCommands_["RENUM"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() > 1) {
                out << "RENUM takes no arguments\n";
                return;
            }
            program_.renumber();
        };
        basicCommands_["NEW"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() > 1) {
                out << "NEW takes no arguments\n";
                return;
            }
            program_.clearLines();
        };
        basicCommands_["LOAD"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() > 2) {
                out << "LOAD takes at most one filename\n";
                return;
            }
            std::optional<std::string> loadName;
            if (tokens.size() == 2) {
                loadName = tokens[1];
            } else {
                loadName = program_.name();
            }
            if (!loadName || loadName->empty()) {
                out << "No program name specified\n";
                return;
            }
            program_.setName(*loadName);
            program_.clearLines();
            loadProgramIfPresent(out);
        };
        basicCommands_["SAVE"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() > 2) {
                out << "SAVE takes at most one filename\n";
                return;
            }
            std::optional<std::string> saveName;
            if (tokens.size() == 2) {
                saveName = tokens[1];
            } else {
                saveName = program_.name();
            }
            if (!saveName || saveName->empty()) {
                out << "No program name specified\n";
                return;
            }
            program_.setName(*saveName);
            saveProgram(*saveName, out);
        };
        basicCommands_["QUIT"] = [this](const Tokens &, std::ostream &, bool &leaveBasicMode) {
            mode_ = Mode::Inactive;
            editorState_.reset();
            leaveBasicMode = true;
        };
        basicCommands_["HELP"] = [](const Tokens &, std::ostream &out, bool &) {
            out << "BASIC commands:\n";
            out << "  <n> <text>     set BASIC line n\n";
            out << "  <n>            delete BASIC line n\n";
            out << "  LIST\n";
            out << "  EDIT <n>\n";
            out << "  DELETE <n>|<n-m>\n";
            out << "  RENUM\n";
            out << "  NEW\n";
            out << "  LOAD [name]\n";
            out << "  SAVE [name]\n";
            out << "  QUIT\n";
        };
    }

    void BasicShell::registerEditorCommands() {
        editorCommands_["FI"] = [this](const Tokens &, std::ostream &, bool &) {
            if (editorState_) {
                program_.setLine(editorState_->lineNumber, editorState_->text);
            }
            editorState_.reset();
            mode_ = Mode::Basic;
        };
        editorCommands_["HELP"] = [](const Tokens &, std::ostream &out, bool &) {
            out << "Editor commands:\n";
            out << "  C/old/new/\n";
            out << "  FI\n";
        };
    }

    void BasicShell::handleBasicCommand(const Tokens &tokens, std::ostream &out, bool &leaveBasicMode) {
        int lineNumber = 0;
        if (parseLineNumber(tokens[0], lineNumber)) {
            if (tokens.size() == 1) {
                (void) program_.deleteLine(lineNumber);
                return;
            }
            program_.setLine(lineNumber, joinTokens(tokens, 1));
            return;
        }

        const auto it = basicCommands_.find(tokens[0]);
        if (it == basicCommands_.end()) {
            out << "Unknown command: " << tokens[0] << "\n";
            return;
        }
        it->second(tokens, out, leaveBasicMode);
    }

    void BasicShell::handleEditorCommand(const Tokens &tokens, std::ostream &out) {
        const auto it = editorCommands_.find(tokens[0]);
        if (it != editorCommands_.end()) {
            bool ignored = false;
            it->second(tokens, out, ignored);
            return;
        }

        if (tokens[0].size() >= 2 && tokens[0][0] == 'C') {
            if (tokens.size() != 1 || !editorState_) {
                out << "Invalid edit command\n";
                return;
            }
            if (!applyEditorSubstitute(editorState_->text, tokens[0])) {
                out << "Invalid edit command\n";
                return;
            }
            program_.setLine(editorState_->lineNumber, editorState_->text);
            return;
        }

        out << "Unknown command: " << tokens[0] << "\n";
    }

    void BasicShell::enterEditorMode(const int lineNumber, std::ostream &out) {
        const std::optional<std::string> line = program_.lineText(lineNumber);
        if (!line) {
            out << "No such line: " << lineNumber << '\n';
            return;
        }
        editorState_ = EditorState{lineNumber, *line};
        mode_ = Mode::Editor;
    }

    void BasicShell::loadProgramIfPresent(std::ostream &out) {
        (void) out;
        if (!program_.name() || program_.name()->empty()) {
            return;
        }
        const std::filesystem::path filePath = programsRoot_ / *program_.name();
        std::error_code ec;
        if (!std::filesystem::exists(filePath, ec)) {
            return;
        }

        std::ifstream file(filePath);
        if (!file) {
            return;
        }

        program_.clearLines();
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            Tokens tokens;
            std::string tok;
            while (iss >> tok) {
                tokens.push_back(tok);
            }
            if (tokens.empty()) {
                continue;
            }
            int lineNumber = 0;
            if (!parseLineNumber(tokens[0], lineNumber)) {
                continue;
            }
            program_.setLine(lineNumber, joinTokens(tokens, 1));
        }
    }

    void BasicShell::saveProgram(const std::string &name, std::ostream &out) {
        std::error_code ec;
        std::filesystem::create_directories(programsRoot_, ec);
        if (ec) {
            out << "Error: unable to create programs directory\n";
            return;
        }

        const std::filesystem::path filePath = programsRoot_ / name;
        std::ofstream file(filePath, std::ios::trunc);
        if (!file) {
            out << "Error: unable to save BASIC program file\n";
            return;
        }
        for (const auto &[lineNumber, text]: program_.lines()) {
            file << lineNumber;
            if (!text.empty()) {
                file << ' ' << text;
            }
            file << '\n';
        }
    }
} // namespace PickShell
