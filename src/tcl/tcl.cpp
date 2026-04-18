//
// Created by Allan McLeod on 18/04/2026.
//

#include "tcl.h"

#include <pick_system/version.hpp>

#include <filesystem>

#include "../core/vm/VM.h"
#include "../core/vm/Instructions.h"

#include <iostream>
#include <sstream>
#include <vector>

namespace PickTCL {

static std::vector<std::string> tokenize(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok)
        out.push_back(tok);
    return out;
}

void repl(PickVM::VM& vm) {
    std::cout << "Pick/TCL minimal shell" << std::endl;
    std::cout << "Type HELP for commands" << std::endl;

    while (true) {
        std::cout << "TCL> " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line))
            break;

        auto tokens = tokenize(line);
        if (tokens.empty())
            continue;

        const std::string& cmd = tokens[0];

        // DUMP-STACK
        if (cmd == "DUMP-STACK") {
            vm.dumpStack();
            continue;
        }

        if (cmd == "LIST-PROGRAMS") {
            std::cout << "Programs:\n";

            std::error_code ec;
            if (!std::filesystem::exists("programs", ec)) {
                std::cout << "  (Directory 'programs' not found in current directory)\n";
                continue;
            }

            for (const auto& entry : std::filesystem::directory_iterator("programs")) {
                if (entry.path().extension() == ".tbc") {
                    std::cout << "  " << entry.path().filename().string() << "\n";
                }
            }
            continue;
        }

        // QUIT
        if (cmd == "QUIT") {
            std::cout << "Exiting TCL\n";
            return;
        }

        // HELP
        if (cmd == "HELP") {
            std::cout << "Commands:\n";
            std::cout << "  ECHO <text>\n";
            std::cout << "  RUN <file>\n";
            std::cout << "  HELP\n";
            std::cout << "  DUMP-STACK\n";
            std::cout << "  LIST-PROGRAMS\n";
            std::cout << "  VERSION\n";
            std::cout << "  QUIT\n";
            continue;
        }

        // VERSION
        if (cmd == "VERSION") {
            std::cout << "pick-system " << pick_system::version_string << "\n";
            continue;
        }

        // ECHO
        if (cmd == "ECHO") {
            if (tokens.size() > 1) {
                for (size_t i = 1; i < tokens.size(); i++)
                    std::cout << tokens[i] << " ";
                std::cout << "\n";
            }
            continue;
        }

        // RUN <file>
        if (cmd == "RUN") {
            if (tokens.size() < 2) {
                std::cout << "RUN requires a filename\n";
                continue;
            }
            const std::string& file = tokens[1];
            try {
                auto program = PickVM::loadTextBytecode(file);
                vm.loadProgram(program);
                vm.run();
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
            continue;
        }

        std::cout << "Unknown command: " << cmd << "\n";
    }
}

} // namespace PickTCL
