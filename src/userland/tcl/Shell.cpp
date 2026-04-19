#include "Shell.h"
#include <pick_system/version.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <optional>
#include <sstream>
#include <filesystem>
#include <string_view>
#include <vector>

namespace PickShell {
    Shell::Shell(PickVM::Runtime &runtime)
        : runtime_(runtime) {
    }

    void Shell::setProgramsRoot(std::filesystem::path root) {
        programsRoot_ = std::move(root);
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
            std::cout << "TCL> " << std::flush;
            std::string line;
            if (!std::getline(std::cin, line))
                break;

            handleLine(line, std::cout, quit);
        }
    }

    void Shell::handleLine(const std::string &line, std::ostream &out, bool &quit) {
        auto tokens = tokenize(line);
        if (!tokens.empty())
            dispatch(tokens, quit, out);
    }

    bool Shell::programImageLoaded() const {
        return lastLoaded_.has_value() && runtime_.isLoaded();
    }

    void Shell::pruneBreakpointsForProgram(const std::size_t programSize, std::ostream &out) {
        std::vector<std::size_t> removed;
        for (auto it = breakpoints_.begin(); it != breakpoints_.end();) {
            if (*it >= programSize) {
                removed.push_back(*it);
                it = breakpoints_.erase(it);
            } else {
                ++it;
            }
        }
        if (!removed.empty()) {
            std::sort(removed.begin(), removed.end());
            out << "Removed invalid breakpoint(s):";
            for (const std::size_t r: removed) {
                out << ' ' << r;
            }
            out << '\n';
        }
    }

    void Shell::executeVmLoop(std::ostream &out) {
        if (!lastLoaded_) {
            return;
        }
        const auto &prog = lastLoaded_->program;
        while (runtime_.instructionPointer() < prog.size()) {
            const std::size_t ip = runtime_.instructionPointer();
            const bool skipBpOnce = resumePastBreakpointIp_ && *resumePastBreakpointIp_ == ip;
            if (skipBpOnce) {
                resumePastBreakpointIp_.reset();
            }

            if (!skipBpOnce && breakpoints_.count(ip) != 0U) {
                out << "Breakpoint hit at " << ip << '\n';
                suspended_ = true;
                break;
            }

            if (trace_) {
                const PickVM::Instruction &instr = prog[ip];
                out << PickVM::formatInstructionLine(ip, instr, &*lastLoaded_) << '\n';
            }

            if (!runtime_.step()) {
                suspended_ = false;
                break;
            }
        }
    }

    void Shell::dispatch(const std::vector<std::string> &tokens, bool &quit, std::ostream &out) {
        const std::string &cmd = tokens[0];

        if (cmd == "QUIT") {
            out << "Exiting shell\n";
            quit = true;
            lastLoaded_.reset();
            suspended_ = false;
            resumePastBreakpointIp_.reset();
            env_.clear();
            runtime_.loadProgram({});
            return;
        }
        if (cmd == "HELP") {
            cmdHelp(out);
            return;
        }
        if (cmd == "VERSION") {
            cmdVersion(out);
            return;
        }
        if (cmd == "ECHO") {
            cmdEcho(tokens, out);
            return;
        }
        if (cmd == "RUN") {
            cmdRun(tokens, out);
            return;
        }
        if (cmd == "DUMP-STACK") {
            cmdDumpStack(out);
            return;
        }
        if (cmd == "LIST-PROGRAMS") {
            cmdListPrograms(out);
            return;
        }
        if (cmd == "TRACE") {
            cmdTrace(tokens, out);
            return;
        }
        if (cmd == "STEP") {
            cmdStep(out);
            return;
        }
        if (cmd == "BREAKPOINT") {
            cmdBreakpoint(tokens, out);
            return;
        }
        if (cmd == "BREAKPOINTS") {
            cmdBreakpoints(out);
            return;
        }
        if (cmd == "CLEAR-BREAKPOINTS") {
            cmdClearBreakpoints(out);
            return;
        }
        if (cmd == "CLEAR-BREAKPOINT") {
            cmdClearBreakpoint(tokens, out);
            return;
        }
        if (cmd == "DUMP-PROGRAM") {
            cmdDumpProgram(out);
            return;
        }
        if (cmd == "DUMP-LABELS") {
            cmdDumpLabels(out);
            return;
        }
        if (cmd == "SET") {
            cmdSet(tokens, out);
            return;
        }
        if (cmd == "GET") {
            cmdGet(tokens, out);
            return;
        }
        if (cmd == "LIST-VARS") {
            cmdListVars(tokens, out);
            return;
        }
        if (cmd == "UNSET") {
            cmdUnset(tokens, out);
            return;
        }

        out << "Unknown command: " << cmd << "\n";
    }

    namespace {
        bool echoVarNameChar(const char c) {
            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
        }
    } // namespace

    std::string Shell::expandEchoToken(const std::string &token) const {
        if (token.size() == 1 && token[0] == '$') {
            return token;
        }
        if (token.size() >= 2 && token[0] == '$') {
            bool ok = true;
            for (std::size_t i = 1; i < token.size(); ++i) {
                if (!echoVarNameChar(token[i])) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                const std::string_view tail(token.data() + 1, token.size() - 1);
                return env_.get(std::string(tail)).value_or("");
            }
        }
        return token;
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
            if (!suspended_) {
                out << "RUN requires a filename\n";
                return;
            }
            if (!programImageLoaded()) {
                suspended_ = false;
                out << "RUN requires a filename\n";
                return;
            }
            resumePastBreakpointIp_ = runtime_.instructionPointer();
            runtime_.setOutputStream(&out);
            executeVmLoop(out);
            runtime_.setOutputStream(nullptr);
            return;
        }

        std::filesystem::path filePath(tokens[1]);
        if (filePath.is_relative() && !std::filesystem::exists(filePath))
            filePath = programsRoot_ / filePath;

        try {
            PickVM::Parser parser;
            PickVM::LoadedBytecode loaded = parser.parseFile(filePath.string());
            pruneBreakpointsForProgram(loaded.program.size(), out);
            lastLoaded_ = std::move(loaded);
            runtime_.setOutputStream(&out);
            runtime_.loadProgram(lastLoaded_->program);
            suspended_ = false;
            resumePastBreakpointIp_.reset();
            executeVmLoop(out);
            runtime_.setOutputStream(nullptr);
        } catch (const std::exception &e) {
            runtime_.setOutputStream(nullptr);
            out << "Error: " << e.what() << "\n";
        }
    }

    void Shell::cmdTrace(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "TRACE requires ON or OFF\n";
            return;
        }
        if (tokens[1] == "ON") {
            trace_ = true;
            return;
        }
        if (tokens[1] == "OFF") {
            trace_ = false;
            return;
        }
        out << "TRACE requires ON or OFF\n";
    }

    void Shell::cmdStep(std::ostream &out) {
        if (!programImageLoaded()) {
            out << "No program loaded\n";
            return;
        }
        if (runtime_.instructionPointer() >= lastLoaded_->program.size()) {
            out << "Program finished\n";
            return;
        }
        suspended_ = false;
        resumePastBreakpointIp_.reset();
        const std::size_t ip = runtime_.instructionPointer();
        const PickVM::Instruction &instr = lastLoaded_->program[ip];
        out << PickVM::formatInstructionLine(ip, instr, &*lastLoaded_) << '\n';
        runtime_.setOutputStream(&out);
        runtime_.step();
        runtime_.setOutputStream(nullptr);
    }

    void Shell::cmdBreakpoint(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "BREAKPOINT requires an index\n";
            return;
        }
        try {
            const std::size_t n = static_cast<std::size_t>(std::stoull(tokens[1]));
            breakpoints_.insert(n);
        } catch (const std::exception &) {
            out << "BREAKPOINT requires a non-negative integer index\n";
        }
    }

    void Shell::cmdBreakpoints(std::ostream &out) {
        if (breakpoints_.empty()) {
            out << "No breakpoints\n";
            return;
        }
        std::vector<std::size_t> sorted(breakpoints_.begin(), breakpoints_.end());
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
            if (breakpoints_.erase(n) == 0U) {
                out << "No such breakpoint\n";
            }
        } catch (const std::exception &) {
            out << "CLEAR-BREAKPOINT requires a non-negative integer index\n";
        }
    }

    void Shell::cmdClearBreakpoints(std::ostream &out) {
        (void) out;
        breakpoints_.clear();
    }

    void Shell::cmdDumpProgram(std::ostream &out) {
        if (!programImageLoaded()) {
            out << "No program loaded\n";
            return;
        }
        for (std::size_t i = 0; i < lastLoaded_->program.size(); ++i) {
            out << PickVM::formatInstructionLine(i, lastLoaded_->program[i], &*lastLoaded_) << '\n';
        }
    }

    void Shell::cmdDumpLabels(std::ostream &out) {
        if (!programImageLoaded()) {
            out << "No program loaded\n";
            return;
        }
        if (lastLoaded_->labels.empty()) {
            out << "No labels\n";
            return;
        }
        std::vector<std::pair<std::string, int>> pairs(lastLoaded_->labels.begin(), lastLoaded_->labels.end());
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
        if (!env_.set(tokens[1], std::move(value))) {
            out << "Invalid variable name\n";
        }
    }

    void Shell::cmdGet(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() < 2) {
            out << "GET requires a variable name\n";
            return;
        }
        const std::optional<std::string> val = env_.get(tokens[1]);
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
        const std::vector<std::string> names = env_.names();
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
        if (!env_.unset(tokens[1])) {
            out << "No such variable\n";
        }
    }

    void Shell::cmdHelp(std::ostream &out) {
        out << "Commands:\n";
        out << "  ECHO <text>   tokens space-separated; $Name expands (unset -> empty)\n";
        out << "  SET <name> <words...>   set variable (names case-insensitive, stored uppercase)\n";
        out << "  GET <name>\n";
        out << "  LIST-VARS\n";
        out << "  UNSET <name>\n";
        out << "  RUN <file>     load and run a .tbc (paths relative to programs root)\n";
        out << "  RUN            resume after a breakpoint (same program in memory)\n";
        out << "  LIST-PROGRAMS\n";
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
        runtime_.setOutputStream(&out);
        runtime_.dumpStack();
        runtime_.setOutputStream(nullptr);
    }

    void Shell::cmdListPrograms(std::ostream &out) {
        std::error_code ec;
        if (!std::filesystem::exists(programsRoot_, ec)) {
            out << "Directory not found: " << programsRoot_.string() << "\n";
            return;
        }

        out << "Programs:\n";
        for (const auto &entry: std::filesystem::directory_iterator(programsRoot_)) {
            if (entry.path().extension() == ".tbc") {
                out << "  " << entry.path().filename().string() << "\n";
            }
        }
    }
} // namespace PickShell
