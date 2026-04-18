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

        auto tokens = tokenize(line);
        if (!tokens.empty())
            dispatch(tokens, quit);
    }
}

void Shell::dispatch(const std::vector<std::string>& tokens, bool& quit) {
    const std::string& cmd = tokens[0];

    if (cmd == "QUIT") {
        std::cout << "Exiting shell\n";
        quit = true;
        return;
    }
    if (cmd == "HELP") {
        cmdHelp();
        return;
    }
    if (cmd == "VERSION") {
        cmdVersion();
        return;
    }
    if (cmd == "ECHO") {
        cmdEcho(tokens);
        return;
    }
    if (cmd == "RUN") {
        cmdRun(tokens);
        return;
    }
    if (cmd == "DUMP-STACK") {
        cmdDumpStack();
        return;
    }
    if (cmd == "LIST-PROGRAMS") {
        cmdListPrograms();
        return;
    }

    std::cout << "Unknown command: " << cmd << "\n";
}

void Shell::cmdEcho(const std::vector<std::string>& tokens) {
    for (size_t i = 1; i < tokens.size(); i++)
        std::cout << tokens[i] << (i == tokens.size() - 1 ? "" : " ");
    std::cout << "\n";
}

void Shell::cmdRun(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        std::cout << "RUN requires a filename\n";
        return;
    }

    const std::string& file = tokens[1];
    try {
        PickVM::Parser parser;
        auto program = parser.parseFile(file);
        runtime_.loadProgram(program);
        runtime_.run();
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

void Shell::cmdHelp() {
    std::cout << "Commands:\n";
    std::cout << "  ECHO <text>\n";
    std::cout << "  RUN <file>\n";
    std::cout << "  LIST-PROGRAMS\n";
    std::cout << "  DUMP-STACK\n";
    std::cout << "  VERSION\n";
    std::cout << "  QUIT\n";
}

void Shell::cmdVersion() {
    std::cout << "Pick System Developer Edition " << pick_system::version_string << "\n";
    std::cout << "Build: " << __DATE__ << "\n";
}

void Shell::cmdDumpStack() {
    runtime_.dumpStack();
}

void Shell::cmdListPrograms() {
    std::error_code ec;
    if (!std::filesystem::exists("programs", ec)) {
        std::cout << "Directory 'programs' not found.\n";
        return;
    }

    std::cout << "Programs:\n";
    for (const auto& entry : std::filesystem::directory_iterator("programs")) {
        if (entry.path().extension() == ".tbc") {
            std::cout << "  " << entry.path().filename().string() << "\n";
        }
    }
}

} // namespace PickShell
