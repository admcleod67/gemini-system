#include "Shell.h"
#include "../core/vm/Parser.h"
#include <pick_system/version.hpp>

#include <iostream>
#include <sstream>
#include <filesystem>
#include <vector>

namespace PickShell {

Shell::Shell(PickVM::Runtime& runtime)
    : runtime_(runtime) {}

void Shell::setProgramsRoot(std::filesystem::path root) {
    programsRoot_ = std::move(root);
}

std::vector<std::string> Shell::tokenize(const std::string& line) {
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

void Shell::handleLine(const std::string& line, std::ostream& out, bool& quit) {
    auto tokens = tokenize(line);
    if (!tokens.empty())
        dispatch(tokens, quit, out);
}

void Shell::dispatch(const std::vector<std::string>& tokens, bool& quit, std::ostream& out) {
    const std::string& cmd = tokens[0];

    if (cmd == "QUIT") {
        out << "Exiting shell\n";
        quit = true;
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

    out << "Unknown command: " << cmd << "\n";
}

void Shell::cmdEcho(const std::vector<std::string>& tokens, std::ostream& out) {
    for (size_t i = 1; i < tokens.size(); i++)
        out << tokens[i] << (i == tokens.size() - 1 ? "" : " ");
    out << "\n";
}

void Shell::cmdRun(const std::vector<std::string>& tokens, std::ostream& out) {
    if (tokens.size() < 2) {
        out << "RUN requires a filename\n";
        return;
    }

    std::filesystem::path filePath(tokens[1]);
    if (filePath.is_relative() && !std::filesystem::exists(filePath))
        filePath = programsRoot_ / filePath;

    try {
        PickVM::Parser parser;
        auto program = parser.parseFile(filePath.string());
        runtime_.setOutputStream(&out);
        runtime_.loadProgram(program);
        runtime_.run();
        runtime_.setOutputStream(nullptr);
    } catch (const std::exception& e) {
        runtime_.setOutputStream(nullptr);
        out << "Error: " << e.what() << "\n";
    }
}

void Shell::cmdHelp(std::ostream& out) {
    out << "Commands:\n";
    out << "  ECHO <text>\n";
    out << "  RUN <file>\n";
    out << "  LIST-PROGRAMS\n";
    out << "  DUMP-STACK\n";
    out << "  VERSION\n";
    out << "  QUIT\n";
}

void Shell::cmdVersion(std::ostream& out) {
    out << "Pick System Developer Edition " << pick_system::version_string << "\n";
    out << "Build: " << __DATE__ << "\n";
}

void Shell::cmdDumpStack(std::ostream& out) {
    runtime_.setOutputStream(&out);
    runtime_.dumpStack();
    runtime_.setOutputStream(nullptr);
}

void Shell::cmdListPrograms(std::ostream& out) {
    std::error_code ec;
    if (!std::filesystem::exists(programsRoot_, ec)) {
        out << "Directory not found: " << programsRoot_.string() << "\n";
        return;
    }

    out << "Programs:\n";
    for (const auto& entry : std::filesystem::directory_iterator(programsRoot_)) {
        if (entry.path().extension() == ".tbc") {
            out << "  " << entry.path().filename().string() << "\n";
        }
    }
}

} // namespace PickShell
