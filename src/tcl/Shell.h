//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_TCL_SHELL_H
#define PICK_SYSTEM_TCL_SHELL_H

#pragma once
#include "../core/vm/Runtime.h"
#include <string>
#include <vector>

namespace PickShell {

class Shell {
public:
    explicit Shell(PickVM::Runtime& runtime);

    void run();  // Start the REPL

private:
    PickVM::Runtime& runtime_;

    // Helpers
    std::vector<std::string> tokenize(const std::string& line);
    void dispatch(const std::vector<std::string>& tokens, bool& quit);

    // Command handlers
    void cmdEcho(const std::vector<std::string>& tokens);
    void cmdRun(const std::vector<std::string>& tokens);
    void cmdHelp();
    void cmdVersion();
    void cmdDumpStack();
    void cmdListPrograms();
};

} // namespace PickShell

#endif //PICK_SYSTEM_TCL_SHELL_H
