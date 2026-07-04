#include <doctest/doctest.h>

#include <sstream>

#include "Runtime.h"
#include "GeminiSession.h"
#include "Shell.h"

TEST_CASE("assembler shell help and command arity") {
    PickShell::GeminiSession gs;
    PickVM::Runtime &rt = gs.runtime();
    PickShell::Shell &sh = gs.shell();
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("ASM", out, quit);
    sh.handleLine("HELP", out, quit);
    CHECK(out.str().find("ASM commands:") != std::string::npos);
    CHECK(out.str().find("CONT") != std::string::npos);

    out.str("");
    sh.handleLine("CONT EXTRA", out, quit);
    CHECK(out.str() == "CONT takes no arguments\n");
}

TEST_CASE("assembler shell END clears vm context without leaving mode") {
    PickShell::GeminiSession gs;
    PickVM::Runtime &rt = gs.runtime();
    PickShell::Shell &sh = gs.shell();
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("ASM", out, quit);
    sh.handleLine("END", out, quit);
    CHECK(out.str() == "No program loaded\n");

    out.str("");
    sh.handleLine("HELP", out, quit);
    CHECK(out.str().find("ASM commands:") != std::string::npos);
}
