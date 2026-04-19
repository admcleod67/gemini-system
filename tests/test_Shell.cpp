#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <pick_system/version.hpp>

#include "Shell.h"
#include "Runtime.h"

static std::filesystem::path uniqueTempDir() {
    auto base = std::filesystem::temp_directory_path();
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return base / ("pick-shell-test-" + std::to_string(tick));
}

TEST_CASE("shell HELP") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("ECHO") != std::string::npos);
    CHECK(out.str().find("RUN") != std::string::npos);
    CHECK(out.str().find("SET") != std::string::npos);
    CHECK(out.str().find("LIST-VARS") != std::string::npos);
}

TEST_CASE("shell VERSION contains project version string") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("VERSION", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find(pick_system::version_string) != std::string::npos);
}

TEST_CASE("shell ECHO spacing") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("ECHO one two", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "one two\n");
}

TEST_CASE("shell QUIT sets quit") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("QUIT", out, quit);
    CHECK(quit);
    CHECK(out.str().find("Exiting") != std::string::npos);
}

TEST_CASE("shell unknown command") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("XYZZY", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Unknown command: XYZZY\n");
}

TEST_CASE("shell SET GET multi-token value") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("SET msg hello world", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().empty());
    out.str("");
    sh.handleLine("GET msg", out, quit);
    CHECK(out.str() == "hello world\n");
}

TEST_CASE("shell SET empty value GET") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("SET empty", out, quit);
    out.str("");
    sh.handleLine("GET empty", out, quit);
    CHECK(out.str() == "\n");
}

TEST_CASE("shell SET requires name") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("SET", out, quit);
    CHECK(out.str() == "SET requires a variable name\n");
}

TEST_CASE("shell GET missing and arity") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("GET", out, quit);
    CHECK(out.str() == "GET requires a variable name\n");
    out.str("");
    sh.handleLine("GET nope", out, quit);
    CHECK(out.str() == "No such variable: nope\n");
}

TEST_CASE("shell LIST-VARS sorted and empty") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-VARS", out, quit);
    CHECK(out.str() == "No variables\n");
    out.str("");
    sh.handleLine("SET z 1", out, quit);
    sh.handleLine("SET a 2", out, quit);
    out.str("");
    sh.handleLine("LIST-VARS", out, quit);
    CHECK(out.str() == "Variables:\n  a\n  z\n");
}

TEST_CASE("shell LIST-VARS takes no arguments") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-VARS extra", out, quit);
    CHECK(out.str() == "LIST-VARS takes no arguments\n");
}

TEST_CASE("shell UNSET") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("SET x y", out, quit);
    out.str("");
    sh.handleLine("UNSET x", out, quit);
    CHECK(out.str().empty());
    out.str("");
    sh.handleLine("GET x", out, quit);
    CHECK(out.str() == "No such variable: x\n");
    out.str("");
    sh.handleLine("UNSET x", out, quit);
    CHECK(out.str() == "No such variable\n");
    out.str("");
    sh.handleLine("UNSET", out, quit);
    CHECK(out.str() == "UNSET requires a variable name\n");
}

TEST_CASE("shell QUIT clears variables") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("SET k v", out, quit);
    sh.handleLine("QUIT", out, quit);
    CHECK(quit);
    quit = false;
    out.str("");
    sh.handleLine("GET k", out, quit);
    CHECK(out.str() == "No such variable: k\n");
}

TEST_CASE("shell RUN requires filename") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("RUN", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "RUN requires a filename\n");
}

TEST_CASE("shell RUN missing file") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(uniqueTempDir());
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("RUN missing.tbc", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("Error:") != std::string::npos);
}

TEST_CASE("shell RUN executes bytecode from programs root") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir); {
        std::ofstream f(dir / "mini.tbc");
        f << "PUSH_INT 5\nHALT\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("RUN mini.tbc", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("Error:") == std::string::npos);
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 5);
}

TEST_CASE("shell LIST-PROGRAMS empty directory") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Programs:\n");
}

TEST_CASE("shell LIST-PROGRAMS lists tbc") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "a.tbc") << "HALT\n";
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("a.tbc") != std::string::npos);
}

TEST_CASE("shell LIST-PROGRAMS missing directory") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(uniqueTempDir());
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("Directory not found:") != std::string::npos);
}

TEST_CASE("shell DUMP-STACK routes dump to out") {
    PickVM::Runtime rt;
    std::vector<PickVM::Instruction> prog = {
        {PickVM::OpCode::PushInt, 9},
        {PickVM::OpCode::Halt, PickVM::Value{}},
    };
    rt.loadProgram(prog);
    rt.run();
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("DUMP-STACK", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Stack: [ 9 ]\n");
}

TEST_CASE("shell STEP DUMP-PROGRAM DUMP-LABELS without loaded program") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("STEP", out, quit);
    CHECK(out.str() == "No program loaded\n");
    out.str("");
    sh.handleLine("DUMP-PROGRAM", out, quit);
    CHECK(out.str() == "No program loaded\n");
    out.str("");
    sh.handleLine("DUMP-LABELS", out, quit);
    CHECK(out.str() == "No program loaded\n");
}

TEST_CASE("shell BREAKPOINT and BREAKPOINTS without RUN") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("BREAKPOINT 0", out, quit);
    CHECK(out.str().empty());
    out.str("");
    sh.handleLine("BREAKPOINTS", out, quit);
    CHECK(out.str() == "Breakpoints: 0\n");
}

TEST_CASE("shell CLEAR-BREAKPOINT and CLEAR-BREAKPOINTS") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("BREAKPOINT 1", out, quit);
    sh.handleLine("BREAKPOINT 2", out, quit);
    sh.handleLine("CLEAR-BREAKPOINT 9", out, quit);
    CHECK(out.str() == "No such breakpoint\n");
    out.str("");
    sh.handleLine("CLEAR-BREAKPOINTS", out, quit);
    sh.handleLine("BREAKPOINTS", out, quit);
    CHECK(out.str() == "No breakpoints\n");
}

TEST_CASE("shell RUN drops out-of-range breakpoints with message") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "tiny.tbc");
        f << "PUSH_INT 1\nHALT\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("BREAKPOINT 5", out, quit);
    sh.handleLine("RUN tiny.tbc", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("Removed invalid breakpoint(s): 5") != std::string::npos);
}

TEST_CASE("shell RUN TRACE ON includes HALT line") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "t.tbc");
        f << "PUSH_INT 7\nHALT\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("TRACE ON", out, quit);
    sh.handleLine("RUN t.tbc", out, quit);
    CHECK(out.str().find("0: PUSH_INT 7") != std::string::npos);
    CHECK(out.str().find("1: HALT") != std::string::npos);
}

TEST_CASE("shell DUMP-PROGRAM and DUMP-LABELS after RUN") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "lbl.tbc");
        f << "start: PUSH_INT 3\nHALT\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("RUN lbl.tbc", out, quit);
    out.str("");
    sh.handleLine("DUMP-PROGRAM", out, quit);
    CHECK(out.str().find("0: PUSH_INT 3") != std::string::npos);
    CHECK(out.str().find("1: HALT") != std::string::npos);
    out.str("");
    sh.handleLine("DUMP-LABELS", out, quit);
    CHECK(out.str().find("start -> 0") != std::string::npos);
}

TEST_CASE("shell breakpoint hit then STEP then RUN completes") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "bp.tbc");
        f << "PUSH_INT 10\nPUSH_INT 20\nADD\nHALT\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("BREAKPOINT 2", out, quit);
    sh.handleLine("RUN bp.tbc", out, quit);
    CHECK(out.str().find("Breakpoint hit at 2") != std::string::npos);
    REQUIRE(rt.stack().size() == 2);
    out.str("");
    sh.handleLine("STEP", out, quit);
    CHECK(out.str().find("2: ADD") != std::string::npos);
    out.str("");
    sh.handleLine("RUN", out, quit);
    CHECK_FALSE(quit);
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 30);
}
