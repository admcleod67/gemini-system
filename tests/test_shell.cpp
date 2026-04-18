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
