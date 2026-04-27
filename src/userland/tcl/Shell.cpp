#include "Shell.h"

#include <pick_system/version.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace PickShell {
    namespace {
        // Set by executeCompiledBasicProgram while a program is running.
        // Accessed from the SIGINT handler, so must be an atomic pointer.
        std::atomic<PickVM::Runtime *> g_interruptRuntime{nullptr};
    } // namespace

    Shell::Shell(PickVM::Runtime &runtime)
        : session_(runtime) {
        session_.runtime_.setFileExistsCallback([this](const std::string &fileName) {
            try {
                (void) session_.fileSystem_.listRecordNames(fileName);
                return true;
            } catch (const PickFS::FileSystemError &e) {
                if (std::string(e.what()).find("File not found: ") == 0) {
                    return false;
                }
                throw;
            }
        });
        session_.runtime_.setReadRecordCallback([this](const std::string &fileName, const std::string &recordName) {
            const std::optional<PickFS::Record> rec = session_.fileSystem_.read(fileName, recordName);
            if (!rec.has_value()) {
                return std::optional<std::string>{};
            }
            return std::optional<std::string>{rec->value()};
        });
        session_.runtime_.setWriteRecordCallback([this](const std::string &fileName, const std::string &recordName, const std::string &value) {
            session_.fileSystem_.write(fileName, PickFS::Record(recordName, value));
        });
        basicShell_.setProgramsRoot(session_.programsRoot());
        basicShell_.setExecuteProgramFn(
            [this](const std::vector<PickVM::Instruction> &program,
                   const std::vector<int> &sourceLinePerInstr,
                   const BasicProgram &sourceProgram,
                   std::ostream &out) {
                executeCompiledBasicProgram(program, sourceLinePerInstr, sourceProgram, out);
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
        std::signal(SIGINT, SIG_IGN); // Ctrl-C does nothing outside a running program
        std::cout << "Gemini/TCL Developer Shell\n";
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
        tclCommands_["WHO"] = [this](const Tokens &, std::ostream &out, bool &) { cmdWho(out); };
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
        tclCommands_["END"] = [this](const Tokens &, std::ostream &out, bool &) { cmdEnd(out); };
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

    void Shell::executeCompiledBasicProgram(const std::vector<PickVM::Instruction> &program,
                                            const std::vector<int> &sourceLinePerInstr,
                                            const BasicProgram &sourceProgram,
                                            std::ostream &out) {
        basicBreakpoints_.clear();
        basicResumePastLine_.reset();
        basicTrace_ = false;

        session_.runtime_.setOutputStream(&out);
        session_.runtime_.setInputStream(inputStream_);
        session_.runtime_.loadProgram(program, sourceLinePerInstr); // also clears interrupted_ flag

        g_interruptRuntime.store(&session_.runtime_);
        const auto previousHandler = std::signal(SIGINT, [](int) {
            if (auto *rt = g_interruptRuntime.load()) {
                rt->interrupt();
            }
        });

        try {
            session_.runtime_.run();
        } catch (const PickVM::UserInterrupt &) {
            out << "\nBreak\n";
            session_.runtime_.clearInterrupt();
            bool done = false;
            while (!done && session_.runtime_.instructionPointer() < program.size()) {
                out << "* " << std::flush;
                std::string line;
                std::istream &in = inputStream_ ? *inputStream_ : std::cin;
                if (!std::getline(in, line)) {
                    session_.runtime_.setInstructionPointer(program.size());
                    break;
                }
                const std::vector<std::string> tokens = tokenizeDebuggerLine(line);
                done = handleBasicDebuggerCommand(tokens, program, sourceLinePerInstr, sourceProgram, out);
            }
        } catch (const std::runtime_error &e) {
            out << "\nRuntime error: " << e.what() << '\n';
            bool done = false;
            while (!done && session_.runtime_.instructionPointer() < program.size()) {
                out << "* " << std::flush;
                std::string line;
                std::istream &in = inputStream_ ? *inputStream_ : std::cin;
                if (!std::getline(in, line)) {
                    session_.runtime_.setInstructionPointer(program.size());
                    break;
                }
                const std::vector<std::string> tokens = tokenizeDebuggerLine(line);
                done = handleBasicDebuggerCommand(tokens, program, sourceLinePerInstr, sourceProgram, out);
            }
        }

        std::signal(SIGINT, previousHandler); // restores SIG_IGN
        g_interruptRuntime.store(nullptr);
        session_.runtime_.setOutputStream(nullptr);
        session_.runtime_.setInputStream(nullptr);
    }

    bool Shell::runBasicUntilStop(const std::vector<PickVM::Instruction> &program,
                                  const std::vector<int> &sourceLinePerInstr,
                                  std::ostream &out,
                                  const bool stopAtNextBasicLine) {
        bool steppedAny = false;
        const int initialLine = session_.runtime_.currentSourceLine();
        while (session_.runtime_.instructionPointer() < program.size()) {
            const std::size_t ip = session_.runtime_.instructionPointer();
            const int sourceLine = (ip < sourceLinePerInstr.size()) ? sourceLinePerInstr[ip] : 0;
            const bool skipLineBreakpointOnce =
                basicResumePastLine_.has_value() && sourceLine > 0 && *basicResumePastLine_ == sourceLine;
            if (skipLineBreakpointOnce) {
                basicResumePastLine_.reset();
            } else if (sourceLine > 0 && basicBreakpoints_.count(sourceLine) != 0U) {
                out << "Breakpoint hit at line " << sourceLine << '\n';
                return false;
            }

            if (basicTrace_) {
                PickVM::LoadedBytecode loaded{program, {}, sourceLinePerInstr};
                out << PickVM::formatInstructionLine(ip, program[ip], &loaded) << '\n';
            }

            try {
                if (!session_.runtime_.step()) {
                    return true;
                }
            } catch (const PickVM::UserInterrupt &) {
                out << "\nBreak\n";
                session_.runtime_.clearInterrupt();
                return false;
            } catch (const std::runtime_error &e) {
                out << "\nRuntime error: " << e.what() << '\n';
                return false;
            }

            steppedAny = true;
            if (stopAtNextBasicLine) {
                const int currentLine = session_.runtime_.currentSourceLine();
                if (currentLine > 0 && currentLine != initialLine && steppedAny) {
                    return false;
                }
            }
        }
        return true;
    }

    std::optional<int> Shell::parsePositiveInt(const std::string &token) {
        try {
            const int value = std::stoi(token);
            if (value > 0) {
                return value;
            }
        } catch (const std::exception &) {
        }
        return std::nullopt;
    }

    std::vector<std::string> Shell::tokenizeDebuggerLine(const std::string &line) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }

    std::set<int> Shell::sourceLinesFromMetadata(const std::vector<int> &sourceLinePerInstr) {
        std::set<int> lines;
        for (const int line: sourceLinePerInstr) {
            if (line > 0) {
                lines.insert(line);
            }
        }
        return lines;
    }

    bool Shell::parseBasicListRange(const std::vector<std::string> &tokens, int &startLine, int &endLine) {
        if (tokens.size() == 1) {
            startLine = 1;
            endLine = std::numeric_limits<int>::max();
            return true;
        }
        if (tokens.size() != 2) {
            return false;
        }
        const std::string &arg = tokens[1];
        const std::size_t dashPos = arg.find('-');
        if (dashPos == std::string::npos) {
            const std::optional<int> line = parsePositiveInt(arg);
            if (!line) {
                return false;
            }
            startLine = *line;
            endLine = *line;
            return true;
        }
        const std::string startToken = arg.substr(0, dashPos);
        const std::string endToken = arg.substr(dashPos + 1);
        const std::optional<int> start = parsePositiveInt(startToken);
        const std::optional<int> end = parsePositiveInt(endToken);
        if (!start || !end || *start > *end) {
            return false;
        }
        startLine = *start;
        endLine = *end;
        return true;
    }

    void Shell::listBasicSourceRange(const BasicProgram &program,
                                     const int startLine,
                                     const int endLine,
                                     std::ostream &out) {
        for (const auto &[lineNumber, text]: program.lines()) {
            if (lineNumber < startLine || lineNumber > endLine) {
                continue;
            }
            out << lineNumber;
            if (!text.empty()) {
                out << ' ' << text;
            }
            out << '\n';
        }
    }

    std::optional<std::size_t> Shell::firstInstructionForSourceLine(const std::vector<int> &sourceLinePerInstr,
                                                                     const int sourceLine) {
        for (std::size_t i = 0; i < sourceLinePerInstr.size(); ++i) {
            if (sourceLinePerInstr[i] == sourceLine) {
                return i;
            }
        }
        return std::nullopt;
    }

    bool Shell::handleBasicDebuggerCommand(const std::vector<std::string> &tokens,
                                           const std::vector<PickVM::Instruction> &program,
                                           const std::vector<int> &sourceLinePerInstr,
                                           const BasicProgram &sourceProgram,
                                           std::ostream &out) {
        if (tokens.empty()) {
            return false;
        }

        const std::string &cmd = tokens[0];
        if (cmd == "CONT") {
            const bool finished = runBasicUntilStop(program, sourceLinePerInstr, out, false);
            return finished;
        }
        if (cmd == "STEP") {
            const bool finished = runBasicUntilStop(program, sourceLinePerInstr, out, true);
            return finished;
        }
        if (cmd == "BREAKPOINT") {
            if (tokens.size() != 2) {
                out << "BREAKPOINT requires a BASIC line number\n";
                return false;
            }
            const std::optional<int> line = parsePositiveInt(tokens[1]);
            if (!line) {
                out << "BREAKPOINT requires a BASIC line number\n";
                return false;
            }
            const std::set<int> knownLines = sourceLinesFromMetadata(sourceLinePerInstr);
            if (knownLines.count(*line) == 0U) {
                out << "No such BASIC line in compiled program\n";
                return false;
            }
            basicBreakpoints_.insert(*line);
            return false;
        }
        if (cmd == "BREAKPOINTS") {
            if (basicBreakpoints_.empty()) {
                out << "No breakpoints\n";
                return false;
            }
            std::vector<int> sorted(basicBreakpoints_.begin(), basicBreakpoints_.end());
            std::sort(sorted.begin(), sorted.end());
            out << "Breakpoints:";
            for (const int line: sorted) {
                out << ' ' << line;
            }
            out << '\n';
            return false;
        }
        if (cmd == "CLEAR-BREAKPOINT") {
            if (tokens.size() != 2) {
                out << "CLEAR-BREAKPOINT requires a BASIC line number\n";
                return false;
            }
            const std::optional<int> line = parsePositiveInt(tokens[1]);
            if (!line) {
                out << "CLEAR-BREAKPOINT requires a BASIC line number\n";
                return false;
            }
            if (basicBreakpoints_.erase(*line) == 0U) {
                out << "No such breakpoint\n";
            }
            return false;
        }
        if (cmd == "CLEAR-BREAKPOINTS") {
            basicBreakpoints_.clear();
            return false;
        }
        if (cmd == "TRACE") {
            if (tokens.size() != 2 || (tokens[1] != "ON" && tokens[1] != "OFF")) {
                out << "TRACE requires ON or OFF\n";
                return false;
            }
            basicTrace_ = (tokens[1] == "ON");
            return false;
        }
        if (cmd == "LIST") {
            int startLine = 1;
            int endLine = std::numeric_limits<int>::max();
            if (!parseBasicListRange(tokens, startLine, endLine)) {
                out << "LIST takes no args, LIST <line>, or LIST <start-end>\n";
                return false;
            }
            listBasicSourceRange(sourceProgram, startLine, endLine, out);
            return false;
        }
        if (cmd == "GOTO") {
            if (tokens.size() != 2) {
                out << "GOTO requires a BASIC line number\n";
                return false;
            }
            const std::optional<int> line = parsePositiveInt(tokens[1]);
            if (!line) {
                out << "GOTO requires a BASIC line number\n";
                return false;
            }
            const std::optional<std::size_t> ip = firstInstructionForSourceLine(sourceLinePerInstr, *line);
            if (!ip) {
                out << "No such BASIC line in compiled program\n";
                return false;
            }
            session_.runtime_.setInstructionPointer(*ip);
            return false;
        }
        if (cmd == "QUIT" || cmd == "END") {
            session_.runtime_.setInstructionPointer(program.size());
            return true;
        }
        if (cmd == "HELP") {
            out << "Debugger commands:\n";
            out << "  CONT\n";
            out << "  STEP\n";
            out << "  BREAKPOINT <line>\n";
            out << "  BREAKPOINTS\n";
            out << "  CLEAR-BREAKPOINT <line>\n";
            out << "  CLEAR-BREAKPOINTS\n";
            out << "  TRACE ON|OFF\n";
            out << "  LIST [line|start-end]\n";
            out << "  GOTO <line>\n";
            out << "  QUIT\n";
            return false;
        }

        out << "Unknown debugger command: " << cmd << '\n';
        return false;
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
        {
            std::error_code ec;
            if (!std::filesystem::exists(filePath, ec) || ec) {
                out << "Error: Cannot open bytecode file: " << filePath.string() << "\n";
                return;
            }
        }

        try {
            PickVM::Parser parser;
            PickVM::LoadedBytecode loaded = parser.parseFile(filePath.string());
            session_.pruneBreakpointsForProgram(loaded.program.size(), out);
            session_.lastLoaded_ = std::move(loaded);
            session_.runtime_.setOutputStream(&out);
            session_.runtime_.setInputStream(inputStream_);
            session_.runtime_.loadProgram(session_.lastLoaded_->program, session_.lastLoaded_->sourceLinePerInstr);
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

    void Shell::cmdEnd(std::ostream &out) {
        if (!session_.programImageLoaded()) {
            out << "No program loaded\n";
            return;
        }
        session_.lastLoaded_.reset();
        session_.suspended_ = false;
        session_.resumePastBreakpointIp_.reset();
        session_.runtime_.loadProgram({});
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
        out << "  WHO\n";
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
        out << "  END            exit VM debugger context\n";
        out << "  DUMP-PROGRAM   disassemble loaded program\n";
        out << "  DUMP-LABELS    list label -> instruction index\n";
        out << "  VERSION\n";
        out << "  QUIT\n";
    }

    void Shell::cmdVersion(std::ostream &out) {
        out << pick_system::system_title << " " << pick_system::version_string << "\n";
        out << "Build: " << __DATE__ << "\n";
    }

    void Shell::cmdWho(std::ostream &out) {
        out << "0 SYSPROG DM\n";
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
