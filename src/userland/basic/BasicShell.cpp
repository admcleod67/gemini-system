#include "BasicShell.h"

#include "BytecodeText.h"

#include <cctype>
#include <sstream>
#include <utility>

namespace PickShell {
    BasicShell::BasicShell() {
        registerCommands();
    }

    void BasicShell::setResolveProgramLocationFn(ResolveProgramLocationFn resolveProgramLocationFn) {
        resolveProgramLocationFn_ = std::move(resolveProgramLocationFn);
    }

    void BasicShell::setReadRecordFn(ReadRecordFn readRecordFn) {
        readRecordFn_ = std::move(readRecordFn);
    }

    void BasicShell::setWriteRecordFn(WriteRecordFn writeRecordFn) {
        writeRecordFn_ = std::move(writeRecordFn);
    }

    void BasicShell::setExecuteProgramFn(ExecuteProgramFn executeProgramFn) {
        executeProgramFn_ = std::move(executeProgramFn);
    }

    void BasicShell::setRunLineRecordEditorFn(RunLineRecordEditorFn runLineRecordEditorFn) {
        runLineRecordEditorFn_ = std::move(runLineRecordEditorFn);
    }

    void BasicShell::enter(std::optional<std::string> programName, std::ostream &out) {
        mode_ = Mode::Basic;
        program_.setName(std::move(programName));
        program_.clearLines();
        loadProgramIfPresent(out);
    }

    bool BasicShell::isActive() const {
        return mode_ != Mode::Inactive;
    }

    std::string BasicShell::prompt() const {
        return "BASIC> ";
    }

    void BasicShell::handleCommand(const Tokens &tokens, std::ostream &out, bool &leaveBasicMode) {
        if (!isActive()) {
            return;
        }
        leaveBasicMode = false;
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

    std::optional<int> BasicShell::physicalRowForBasicLine(const int basicLineNumber) const {
        int row = 0;
        for (const auto &[k, v]: program_.lines()) {
            (void) v;
            ++row;
            if (k == basicLineNumber) {
                return row;
            }
        }
        return std::nullopt;
    }

    bool BasicShell::flushProgramSourceToDisk(std::ostream &out) {
        if (!program_.name() || program_.name()->empty()) {
            out << "No program name\n";
            return false;
        }
        if (!resolveProgramLocationFn_ || !writeRecordFn_) {
            out << "Error: BASIC storage not configured\n";
            return false;
        }
        std::ostringstream source;
        for (const auto &[lineNumber, text]: program_.lines()) {
            source << lineNumber;
            if (!text.empty()) {
                source << ' ' << text;
            }
            source << '\n';
        }

        std::string error;
        const ProgramLocation location = resolveProgramLocationFn_(*program_.name());
        if (!writeRecordFn_(location, false, source.str(), error)) {
            out << "Error: " << error << "\n";
            return false;
        }
        return true;
    }

    void BasicShell::registerCommands() {
        registerBasicCommands();
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
            if (tokens.size() > 2) {
                out << "EDIT takes no arguments or one line number\n";
                return;
            }
            if (!program_.name() || program_.name()->empty()) {
                out << "No program name\n";
                return;
            }
            std::optional<int> highlightPhysical;
            if (tokens.size() == 2) {
                int basicLine = 0;
                if (!parseLineNumber(tokens[1], basicLine)) {
                    out << "EDIT requires a line number\n";
                    return;
                }
                if (!program_.hasLine(basicLine)) {
                    out << "No such line: " << basicLine << '\n';
                    return;
                }
                highlightPhysical = physicalRowForBasicLine(basicLine);
            }
            if (!runLineRecordEditorFn_) {
                out << "Error: line editor not configured\n";
                return;
            }
            if (!flushProgramSourceToDisk(out)) {
                return;
            }
            const ProgramLocation location = resolveProgramLocationFn_(*program_.name());
            runLineRecordEditorFn_(location, highlightPhysical, out);
            loadProgramIfPresent(out);
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
        basicCommands_["RENUMBER"] = basicCommands_["RENUM"];
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
                out << "No program name\n";
                return;
            }
            program_.setName(*saveName);
            (void) flushProgramSourceToDisk(out);
        };
        basicCommands_["COMPILE"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() > 1) {
                out << "COMPILE takes no arguments\n";
                return;
            }
            if (!program_.name() || program_.name()->empty()) {
                out << "No program name\n";
                return;
            }
            const BasicCompileResult compile = compiler_.compile(program_);
            if (!compile.success) {
                printCompileFailure(compile, out);
                return;
            }
            if (!saveObjectCode(*program_.name(), compile, out)) {
                return;
            }
            printCompileSuccess(compile, out);
        };
        basicCommands_["RUN"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            const bool compileOnly = tokens.size() == 2 && tokens[1] == "(C";
            if (!(tokens.size() == 1 || compileOnly)) {
                out << "RUN takes no arguments or (C\n";
                return;
            }
            if (!program_.name() || program_.name()->empty()) {
                out << "No program name\n";
                return;
            }
            const BasicCompileResult compile = compiler_.compile(program_);
            if (!compile.success) {
                printCompileFailure(compile, out);
                return;
            }
            if (compileOnly) {
                if (!saveObjectCode(*program_.name(), compile, out)) {
                    return;
                }
                printCompileSuccess(compile, out);
                return;
            }
            if (!executeProgramFn_) {
                out << "Error: BASIC runtime not configured\n";
                return;
            }
            executeProgramFn_(compile.program, compile.sourceLinePerInstr, program_, out);
        };
        basicCommands_["QUIT"] = [this](const Tokens &, std::ostream &, bool &leaveBasicMode) {
            mode_ = Mode::Inactive;
            leaveBasicMode = true;
        };
        basicCommands_["HELP"] = [](const Tokens &, std::ostream &out, bool &) {
            out << "BASIC commands:\n";
            out << "  <n> <text>     set BASIC line n\n";
            out << "  <n>            delete BASIC line n\n";
            out << "  LIST\n";
            out << "  EDIT [n]       open system line editor (TCL EDIT); optional BASIC line n for context\n";
            out << "  DELETE <n>|<n-m>\n";
            out << "  RENUM\n";
            out << "  RENUMBER\n";
            out << "  NEW\n";
            out << "  LOAD [name]\n";
            out << "  SAVE [name]\n";
            out << "  COMPILE\n";
            out << "  RUN\n";
            out << "  RUN (C\n";
            out << "  QUIT\n";
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

    void BasicShell::loadProgramIfPresent(std::ostream &out) {
        (void) out;
        if (!program_.name() || program_.name()->empty() || !resolveProgramLocationFn_ || !readRecordFn_) {
            return;
        }
        const ProgramLocation location = resolveProgramLocationFn_(*program_.name());
        const std::optional<std::string> source = readRecordFn_(location, false);
        if (!source.has_value()) {
            return;
        }
        program_.clearLines();
        std::istringstream in(*source);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream iss(line);
            Tokens tokens;
            std::string tok;
            while (iss >> tok) {
                tokens.push_back(tok);
            }
            if (tokens.empty()) {
                continue;
            }
            int ln = 0;
            if (!parseLineNumber(tokens[0], ln)) {
                continue;
            }
            program_.setLine(ln, joinTokens(tokens, 1));
        }
    }

    void BasicShell::saveProgram(const std::string &name, std::ostream &out) {
        if (!resolveProgramLocationFn_ || !writeRecordFn_) {
            out << "Error: BASIC storage not configured\n";
            return;
        }
        program_.setName(name);
        (void) flushProgramSourceToDisk(out);
    }

    bool BasicShell::saveObjectCode(const std::string &name, const BasicCompileResult &compile, std::ostream &out) {
        if (!resolveProgramLocationFn_ || !writeRecordFn_) {
            out << "Error: BASIC storage not configured\n";
            return false;
        }
        std::string error;
        const ProgramLocation location = resolveProgramLocationFn_(name);
        if (!writeRecordFn_(location,
                            true,
                            PickVM::serializeBytecodeText(compile.program, compile.sourceLinePerInstr),
                            error)) {
            out << "Error: " << error << "\n";
            return false;
        }
        return true;
    }

    void BasicShell::printCompileSuccess(const BasicCompileResult &compile, std::ostream &out) {
        out << "Compiled successfully.\n";
        out << "Instructions: " << compile.instructionCount << '\n';
        out << "Labels: " << compile.labelCount << '\n';
    }

    void BasicShell::printCompileFailure(const BasicCompileResult &compile, std::ostream &out) {
        for (const auto &err: compile.errors) {
            out << "Error on line " << err.line << ": " << err.message << '\n';
        }
        out << "Compilation failed.\n";
    }
} // namespace PickShell
