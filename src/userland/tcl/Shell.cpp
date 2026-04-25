#include "Shell.h"

#include <pick_system/version.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <vector>

namespace PickShell {
    Shell::Shell(PickVM::Runtime &runtime)
        : session_(runtime) {
        basicShell_.setProgramsRoot(session_.programsRoot());
        basicShell_.setExecuteProgramFn(
            [this](const std::vector<PickVM::Instruction> &program, std::ostream &out) {
                executeCompiledBasicProgram(program, out);
            });
        registerCommands();
    }

    void Shell::setProgramsRoot(std::filesystem::path root) {
        session_.setProgramsRoot(std::move(root));
        basicShell_.setProgramsRoot(session_.programsRoot());
    }

    void Shell::setFileSystemRoot(std::filesystem::path root) {
        session_.setFileSystemRoot(std::move(root));
    }

    std::vector<std::string> Shell::tokenize(const std::string &line) {
        std::istringstream iss(line);
        std::vector<std::string> out;
        std::string tok;
        while (iss >> tok)
            out.push_back(tok);
        return out;
    }

    void Shell::run() {
        std::cout << "Pick/TCL Developer Shell\n";
        std::cout << "Type HELP for commands\n";

        bool quit = false;
        while (!quit) {
            std::cout << prompt() << std::flush;
            std::string line;
            if (!std::getline(std::cin, line))
                break;
            handleLine(line, std::cout, quit);
        }
    }

    void Shell::handleLine(const std::string &line, std::ostream &out, bool &quit) {
        auto tokens = tokenize(line);
        if (!tokens.empty()) {
            dispatch(tokens, quit, out);
        }
    }

    void Shell::setInputStream(std::istream *in) {
        inputStream_ = in;
    }

    std::string Shell::prompt() const {
        if (basicShell_.isActive()) {
            return basicShell_.prompt();
        }
        return "TCL> ";
    }

    void Shell::registerCommands() {
        registerTclCommands();
    }

    void Shell::registerTclCommands() {
        tclCommands_["QUIT"] = [this](const Tokens &, std::ostream &out, bool &quit) {
            out << "Exiting shell\n";
            quit = true;
            session_.resetForQuit();
        };
        tclCommands_["HELP"] = [this](const Tokens &, std::ostream &out, bool &) { cmdHelp(out); };
        tclCommands_["VERSION"] = [this](const Tokens &, std::ostream &out, bool &) { cmdVersion(out); };
        tclCommands_["ECHO"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdEcho(tokens, out); };
        tclCommands_["RUN"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdRun(tokens, out); };
        tclCommands_["DUMP-STACK"] = [this](const Tokens &, std::ostream &out, bool &) { cmdDumpStack(out); };
        tclCommands_["LIST-PROGRAMS"] = [this](const Tokens &, std::ostream &out, bool &) { cmdListPrograms(out); };
        tclCommands_["CREATE-FILE"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdCreateFile(tokens, out);
        };
        tclCommands_["DELETE-FILE"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdDeleteFile(tokens, out);
        };
        tclCommands_["LIST-FILES"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdListFiles(tokens, out);
        };
        tclCommands_["LIST"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdList(tokens, out); };
        tclCommands_["READ"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdRead(tokens, out); };
        tclCommands_["WRITE"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdWrite(tokens, out); };
        tclCommands_["TRACE"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdTrace(tokens, out); };
        tclCommands_["STEP"] = [this](const Tokens &, std::ostream &out, bool &) { cmdStep(out); };
        tclCommands_["BREAKPOINT"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdBreakpoint(tokens, out);
        };
        tclCommands_["BREAKPOINTS"] = [this](const Tokens &, std::ostream &out, bool &) { cmdBreakpoints(out); };
        tclCommands_["CLEAR-BREAKPOINT"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdClearBreakpoint(tokens, out);
        };
        tclCommands_["CLEAR-BREAKPOINTS"] = [this](const Tokens &, std::ostream &out, bool &) {
            cmdClearBreakpoints(out);
        };
        tclCommands_["DUMP-PROGRAM"] = [this](const Tokens &, std::ostream &out, bool &) { cmdDumpProgram(out); };
        tclCommands_["DUMP-LABELS"] = [this](const Tokens &, std::ostream &out, bool &) { cmdDumpLabels(out); };
        tclCommands_["SET"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdSet(tokens, out); };
        tclCommands_["GET"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdGet(tokens, out); };
        tclCommands_["LIST-VARS"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            cmdListVars(tokens, out);
        };
        tclCommands_["UNSET"] = [this](const Tokens &tokens, std::ostream &out, bool &) { cmdUnset(tokens, out); };
        tclCommands_["BASIC"] = [this](const Tokens &tokens, std::ostream &out, bool &) {
            if (tokens.size() > 2) {
                out << "BASIC takes at most one program name\n";
                return;
            }
            std::optional<std::string> programName;
            if (tokens.size() == 2) {
                programName = tokens[1];
            }
            basicShell_.enter(programName, out);
        };
    }

    void Shell::dispatch(const std::vector<std::string> &tokens, bool &quit, std::ostream &out) {
        if (basicShell_.isActive()) {
            bool leaveBasic = false;
            basicShell_.handleCommand(tokens, out, leaveBasic);
            return;
        }
        handleTclCommand(tokens, quit, out);
    }

    void Shell::handleTclCommand(const Tokens &tokens, bool &quit, std::ostream &out) {
        const auto it = tclCommands_.find(tokens[0]);
        if (it == tclCommands_.end()) {
            out << "Unknown command: " << tokens[0] << "\n";
            return;
        }
        it->second(tokens, out, quit);
    }

    void Shell::executeCompiledBasicProgram(const std::vector<PickVM::Instruction> &program, std::ostream &out) {
        session_.runtime_.setOutputStream(&out);
        session_.runtime_.setInputStream(inputStream_);
        session_.runtime_.loadProgram(program);
        try {
            session_.runtime_.run();
        } catch (const std::runtime_error &e) {
            out << "\nRuntime error: " << e.what() << '\n';
        }
        session_.runtime_.setOutputStream(nullptr);
        session_.runtime_.setInputStream(nullptr);
    }

    namespace {
        bool echoVarNameChar(const char c) {
            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
        }
    } // namespace

    std::string Shell::expandEchoToken(const std::string &token) const {
        std::string out;
        out.reserve(token.size());
        for (std::size_t i = 0; i < token.size();) {
            if (token[i] == '$' && i + 1 < token.size() && token[i + 1] == '$') {
                out.push_back('$');
                i += 2;
                continue;
            }
            if (token[i] == '$' && i + 1 < token.size() && echoVarNameChar(token[i + 1])) {
                std::size_t j = i + 1;
                while (j < token.size() && echoVarNameChar(token[j])) {
                    ++j;
                }
                const std::string name(token.data() + i + 1, j - i - 1);
                out += session_.env_.get(name).value_or("");
                i = j;
                continue;
            }
            if (token[i] == '$') {
                out.push_back('$');
                ++i;
                continue;
            }
            out.push_back(token[i]);
            ++i;
        }
        return out;
    }

    void Shell::cmdEcho(const std::vector<std::string> &tokens, std::ostream &out) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            out << expandEchoToken(tokens[i]);
            if (i + 1 < tokens.size()) {
                out << ' ';
            }
        }
        out << "\n";
    }

    void Shell::cmdRun(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() == 1) {
            if (!session_.suspended_) {
                out << "RUN requires a filename\n";
                return;
            }
            if (!session_.programImageLoaded()) {
                session_.suspended_ = false;
                out << "RUN requires a filename\n";
                return;
            }
            session_.resumePastBreakpointIp_ = session_.runtime_.instructionPointer();
            session_.runtime_.setOutputStream(&out);
            session_.runtime_.setInputStream(inputStream_);
            session_.executeVmLoop(out);
            session_.runtime_.setOutputStream(nullptr);
            session_.runtime_.setInputStream(nullptr);
            return;
        }

        std::filesystem::path filePath(tokens[1]);
        if (filePath.is_relative() && !std::filesystem::exists(filePath))
            filePath = session_.programsRoot_ / filePath;

        try {
            PickVM::Parser parser;
            PickVM::LoadedBytecode loaded = parser.parseFile(filePath.string());
            session_.pruneBreakpointsForProgram(loaded.program.size(), out);
            session_.lastLoaded_ = std::move(loaded);
            session_.runtime_.setOutputStream(&out);
            session_.runtime_.setInputStream(inputStream_);
            session_.runtime_.loadProgram(session_.lastLoaded_->program);
            session_.suspended_ = false;
            session_.resumePastBreakpointIp_.reset();
            session_.executeVmLoop(out);
            session_.runtime_.setOutputStream(nullptr);
            session_.runtime_.setInputStream(nullptr);
        } catch (const std::exception &e) {
            session_.runtime_.setOutputStream(nullptr);
            session_.runtime_.setInputStream(nullptr);
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdTrace(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "TRACE requires ON or OFF\n";
            return;
        }
        if (tokens[1] == "ON") {
            session_.trace_ = true;
            return;
        }
        if (tokens[1] == "OFF") {
            session_.trace_ = false;
            return;
        }
        out << "TRACE requires ON or OFF\n";
    }

    void Shell::cmdStep(std::ostream &out) {
        if (!session_.programImageLoaded()) {
            out << "No program loaded\n";
            return;
        }
        if (session_.runtime_.instructionPointer() >= session_.lastLoaded_->program.size()) {
            out << "Program finished\n";
            return;
        }
        session_.suspended_ = false;
        session_.resumePastBreakpointIp_.reset();
        const std::size_t ip = session_.runtime_.instructionPointer();
        const PickVM::Instruction &instr = session_.lastLoaded_->program[ip];
        out << PickVM::formatInstructionLine(ip, instr, &*session_.lastLoaded_) << '\n';
        session_.runtime_.setOutputStream(&out);
        session_.runtime_.step();
        session_.runtime_.setOutputStream(nullptr);
    }

    void Shell::cmdBreakpoint(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "BREAKPOINT requires an index\n";
            return;
        }
        try {
            const std::size_t n = static_cast<std::size_t>(std::stoull(tokens[1]));
            session_.breakpoints_.insert(n);
        } catch (const std::exception &) {
            out << "BREAKPOINT requires a non-negative integer index\n";
        }
    }

    void Shell::cmdBreakpoints(std::ostream &out) {
        if (session_.breakpoints_.empty()) {
            out << "No breakpoints\n";
            return;
        }
        std::vector<std::size_t> sorted(session_.breakpoints_.begin(), session_.breakpoints_.end());
        std::sort(sorted.begin(), sorted.end());
        out << "Breakpoints:";
        for (const std::size_t b: sorted) {
            out << ' ' << b;
        }
        out << '\n';
    }

    void Shell::cmdClearBreakpoint(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "CLEAR-BREAKPOINT requires an index\n";
            return;
        }
        try {
            const std::size_t n = static_cast<std::size_t>(std::stoull(tokens[1]));
            if (session_.breakpoints_.erase(n) == 0U) {
                out << "No such breakpoint\n";
            }
        } catch (const std::exception &) {
            out << "CLEAR-BREAKPOINT requires a non-negative integer index\n";
        }
    }

    void Shell::cmdClearBreakpoints(std::ostream &out) {
        (void) out;
        session_.breakpoints_.clear();
    }

    void Shell::cmdDumpProgram(std::ostream &out) {
        if (!session_.programImageLoaded()) {
            out << "No program loaded\n";
            return;
        }
        for (std::size_t i = 0; i < session_.lastLoaded_->program.size(); ++i) {
            out << PickVM::formatInstructionLine(i, session_.lastLoaded_->program[i], &*session_.lastLoaded_) << '\n';
        }
    }

    void Shell::cmdDumpLabels(std::ostream &out) {
        if (!session_.programImageLoaded()) {
            out << "No program loaded\n";
            return;
        }
        if (session_.lastLoaded_->labels.empty()) {
            out << "No labels\n";
            return;
        }
        std::vector<std::pair<std::string, int>> pairs(session_.lastLoaded_->labels.begin(), session_.lastLoaded_->labels.end());
        std::sort(pairs.begin(), pairs.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &p: pairs) {
            out << p.first << " -> " << p.second << '\n';
        }
    }

    void Shell::cmdSet(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "SET requires a variable name\n";
            return;
        }
        std::string value;
        for (size_t i = 2; i < tokens.size(); ++i) {
            if (i > 2) {
                value += ' ';
            }
            value += tokens[i];
        }
        if (!session_.env_.set(tokens[1], std::move(value))) {
            out << "Invalid variable name\n";
        }
    }

    void Shell::cmdGet(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "GET requires a variable name\n";
            return;
        }
        const std::optional<std::string> val = session_.env_.get(tokens[1]);
        if (!val) {
            out << "No such variable: " << tokens[1] << '\n';
            return;
        }
        out << *val << '\n';
    }

    void Shell::cmdListVars(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() > 1) {
            out << "LIST-VARS takes no arguments\n";
            return;
        }
        const std::vector<std::string> names = session_.env_.names();
        if (names.empty()) {
            out << "No variables\n";
            return;
        }
        out << "Variables:\n";
        for (const std::string &n: names) {
            out << "  " << n << '\n';
        }
    }

    void Shell::cmdUnset(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "UNSET requires a variable name\n";
            return;
        }
        if (!session_.env_.unset(tokens[1])) {
            out << "No such variable\n";
        }
    }

    void Shell::cmdHelp(std::ostream &out) {
        out << "Commands:\n";
        out << "  ECHO <text>   $Name expands; $$ -> $; other $ literal; unset -> empty\n";
        out << "  SET <name> <words...>   set variable (names case-insensitive, stored uppercase)\n";
        out << "  GET <name>\n";
        out << "  LIST-VARS\n";
        out << "  UNSET <name>\n";
        out << "  BASIC [name]   enter BASIC editor mode\n";
        out << "  RUN <file>     load and run a .tbc (paths relative to programs root)\n";
        out << "  RUN            resume after a breakpoint (same program in memory)\n";
        out << "  LIST-PROGRAMS\n";
        out << "  CREATE-FILE <name>\n";
        out << "  DELETE-FILE <name>\n";
        out << "  LIST-FILES\n";
        out << "  LIST <file>    list record names for a file\n";
        out << "  READ <file> <record-name>\n";
        out << "  WRITE <file> <record-name> <value...>\n";
        out << "  DUMP-STACK\n";
        out << "  TRACE ON|OFF   trace each instruction before execution (when running)\n";
        out << "  STEP           single-step (requires loaded program)\n";
        out << "  BREAKPOINT <n> set breakpoint at instruction index (may be set before RUN)\n";
        out << "  BREAKPOINTS    list breakpoint indices\n";
        out << "  CLEAR-BREAKPOINT <n>\n";
        out << "  CLEAR-BREAKPOINTS\n";
        out << "  DUMP-PROGRAM   disassemble loaded program\n";
        out << "  DUMP-LABELS    list label -> instruction index\n";
        out << "  VERSION\n";
        out << "  QUIT\n";
    }

    void Shell::cmdVersion(std::ostream &out) {
        out << pick_system::system_title << " " << pick_system::version_string << "\n";
        out << "Build: " << __DATE__ << "\n";
    }

    void Shell::cmdDumpStack(std::ostream &out) {
        session_.runtime_.setOutputStream(&out);
        session_.runtime_.dumpStack();
        session_.runtime_.setOutputStream(nullptr);
    }

    void Shell::cmdListPrograms(std::ostream &out) {
        std::error_code ec;
        if (!std::filesystem::exists(session_.programsRoot_, ec)) {
            out << "Directory not found: " << session_.programsRoot_.string() << "\n";
            return;
        }

        out << "Programs:\n";
        for (const auto &entry: std::filesystem::directory_iterator(session_.programsRoot_)) {
            if (entry.path().extension() == ".tbc") {
                out << "  " << entry.path().filename().string() << "\n";
            }
        }
    }

    void Shell::cmdCreateFile(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "CREATE-FILE requires a filename\n";
            return;
        }
        try {
            session_.fileSystem_.createFile(tokens[1]);
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdDeleteFile(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "DELETE-FILE requires a filename\n";
            return;
        }
        try {
            session_.fileSystem_.deleteFile(tokens[1]);
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdListFiles(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() > 1) {
            out << "LIST-FILES takes no arguments\n";
            return;
        }
        try {
            const std::vector<std::string> names = session_.fileSystem_.listFiles();
            if (names.empty()) {
                out << "No files\n";
                return;
            }
            out << "Files:\n";
            for (const std::string &name: names) {
                out << "  " << name << '\n';
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdList(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 2) {
            out << "LIST requires a filename\n";
            return;
        }
        try {
            const std::vector<std::string> names = session_.fileSystem_.listRecordNames(tokens[1]);
            if (names.empty()) {
                out << "No records\n";
                return;
            }
            out << "Records:\n";
            for (const std::string &name: names) {
                out << "  " << name << '\n';
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdRead(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 3) {
            out << "READ requires a file and record name\n";
            return;
        }
        try {
            const std::optional<PickFS::Record> record = session_.fileSystem_.read(tokens[1], tokens[2]);
            if (!record) {
                out << "No such record\n";
                return;
            }
            out << record->value() << '\n';
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdWrite(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 4) {
            out << "WRITE requires a file, record name, and value\n";
            return;
        }
        std::string value;
        for (std::size_t i = 3; i < tokens.size(); ++i) {
            if (i > 3) {
                value += ' ';
            }
            value += tokens[i];
        }
        try {
            session_.fileSystem_.write(tokens[1], PickFS::Record(tokens[2], std::move(value)));
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << "\n";
        }
    }
} // namespace PickShell
