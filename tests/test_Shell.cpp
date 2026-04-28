#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

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
    CHECK(out.str().find("ASM") != std::string::npos);
    CHECK(out.str().find("SET") != std::string::npos);
    CHECK(out.str().find("LIST-VARS") != std::string::npos);
    CHECK(out.str().find("WHO") != std::string::npos);
}

TEST_CASE("shell WHO returns default port user account line") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("WHO", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "0 SYSPROG DM\n");
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

TEST_CASE("shell ECHO variable substitution") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("SET GREETING hello", out, quit);
    out.str("");
    sh.handleLine("ECHO $greeting", out, quit);
    CHECK(out.str() == "hello\n");
}

TEST_CASE("shell ECHO two variables and plus") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("SET A 10", out, quit);
    sh.handleLine("SET B 20", out, quit);
    out.str("");
    sh.handleLine("ECHO $A + $B", out, quit);
    CHECK(out.str() == "10 + 20\n");
}

TEST_CASE("shell ECHO unset variable empty and literal dollar") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("SET a x", out, quit);
    out.str("");
    sh.handleLine("ECHO $a $missing $a", out, quit);
    CHECK(out.str() == "x  x\n");
    out.str("");
    sh.handleLine("ECHO $", out, quit);
    CHECK(out.str() == "$\n");
    out.str("");
    sh.handleLine("ECHO $$A", out, quit);
    CHECK(out.str() == "$A\n");
    out.str("");
    sh.handleLine("ECHO $$", out, quit);
    CHECK(out.str() == "$\n");
    out.str("");
    sh.handleLine("ECHO $bad-name", out, quit);
    CHECK(out.str() == "-name\n");
    out.str("");
    sh.handleLine("SET bad x", out, quit);
    sh.handleLine("ECHO $bad-name", out, quit);
    CHECK(out.str() == "x-name\n");
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
    sh.handleLine("GET MSG", out, quit);
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
    CHECK(out.str() == "Variables:\n  A\n  Z\n");
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
    sh.handleLine("RUN missing", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("Error: Cannot open bytecode file:") != std::string::npos);
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
    sh.handleLine("RUN mini", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("Error:") == std::string::npos);
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 5);
}

TEST_CASE("shell RUN rejects extension-bearing names") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("RUN mini.tbc", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "RUN expects a program name without extension\n");
}

TEST_CASE("shell RUN auto-compiles BASIC source when bytecode missing") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream src(dir / "HELLO");
        src << "10 PRINT \"HI\"\n20 END\n";
    }

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;

    CHECK_FALSE(std::filesystem::exists(dir / "HELLO.tbc"));
    sh.handleLine("RUN HELLO", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "HI\n");
    CHECK(std::filesystem::exists(dir / "HELLO.tbc"));
}

TEST_CASE("shell RUN compile failure from BASIC source reports diagnostics") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream src(dir / "BROKEN");
        src << "10 MAT A = 1\n";
    }

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("RUN BROKEN", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Error on line 10: Unknown keyword 'MAT'\nCompilation failed.\n");
    CHECK_FALSE(std::filesystem::exists(dir / "BROKEN.tbc"));
}

TEST_CASE("shell PROC requires program name") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "PROC requires a program name\n");
}

TEST_CASE("shell PROC missing file") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC MISSING", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("Error: Cannot open PROC file:") != std::string::npos);
}

TEST_CASE("shell PROC assignment display and token substitution") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "FLOW.proc");
        f << "MSG = NOOP\n";
        f << "X = HELLO\n";
        f << "DISPLAY X WORLD\n";
        f << "DISPLAY MSG\n";
        f << "END\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC FLOW", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "HELLO WORLD\nNOOP\n");
}

TEST_CASE("shell PROC INPUT stores value and can be displayed") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "ASK.proc");
        f << "INPUT USER\n";
        f << "DISPLAY USER\n";
        f << "END\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::istringstream in("ALICE\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC ASK", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "ALICE\n");
}

TEST_CASE("shell PROC GO supports forward and backward labels") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "JUMP.proc");
        f << "GO NEXT\n";
        f << "DISPLAY BAD\n";
        f << "NEXT:\n";
        f << "DISPLAY OK\n";
        f << "END\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC JUMP", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "OK\n");
}

TEST_CASE("shell PROC IF THEN GO with case-insensitive labels") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "IFLOW.proc");
        f << "X = YES\n";
        f << "IF X = YES THEN GO menu\n";
        f << "DISPLAY FAIL\n";
        f << "MENU:\n";
        f << "DISPLAY PASS\n";
        f << "END\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC IFLOW", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "PASS\n");
}

TEST_CASE("shell PROC positional parameters substitute into TCL command") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "BRIDGE.proc");
        f << "TCL ECHO P1 P2\n";
        f << "END\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC BRIDGE HELLO WORLD", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "HELLO WORLD\n");
}

TEST_CASE("shell PROC duplicate labels are rejected") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "DUP.proc");
        f << "A:\n";
        f << "A:\n";
        f << "END\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC DUP", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Error: Duplicate label: A\n");
}

TEST_CASE("shell PROC malformed IF and unknown label errors") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);

    {
        std::ofstream f(dir / "BADIF.proc");
        f << "IF A = B THEN DISPLAY X\n";
        f << "END\n";
    }
    {
        std::ofstream f(dir / "BADGO.proc");
        f << "GO MISSING\n";
        f << "END\n";
    }

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC BADIF", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Error: IF requires IF <lhs> = <rhs> THEN GO <label>\n");

    out.str("");
    sh.handleLine("PROC BADGO", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Error: Unknown label: MISSING\n");
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

TEST_CASE("shell LIST-PROGRAMS lists logical names from tbc and bas") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "a.tbc") << "HALT\n";
    std::ofstream(dir / "b.bas") << "10 END\n";
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Programs:\n  a\n  b\n");
}

TEST_CASE("shell LIST-PROGRAMS deduplicates bas and tbc with same basename") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "HELLO.bas") << "10 PRINT \"HI\"\n";
    std::ofstream(dir / "HELLO.tbc") << "HALT\n";
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Programs:\n  HELLO\n");
}

TEST_CASE("shell LIST-PROGRAMS matches extension case-insensitively and sorts names") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "zeta.TBC") << "HALT\n";
    std::ofstream(dir / "alpha.BAS") << "10 END\n";
    std::ofstream(dir / "ignore.tmp") << "x\n";
    std::filesystem::create_directory(dir / "folder.tbc");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Programs:\n  alpha\n  zeta\n");
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

TEST_CASE("shell CREATE-FILE LIST-FILES DELETE-FILE") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("LIST-FILES", out, quit);
    CHECK(out.str() == "No files\n");

    out.str("");
    sh.handleLine("CREATE-FILE BP", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("LIST-FILES", out, quit);
    CHECK(out.str() == "Files:\n  BP\n");

    out.str("");
    sh.handleLine("DELETE-FILE BP", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("LIST-FILES", out, quit);
    CHECK(out.str() == "No files\n");
}

TEST_CASE("shell WRITE READ and overwrite record value") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-FILE BP", out, quit);
    out.str("");
    sh.handleLine("WRITE BP HELLO 001*first value", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("READ BP HELLO", out, quit);
    CHECK(out.str() == "001*first value\n");

    out.str("");
    sh.handleLine("WRITE BP HELLO 002*second", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("READ BP HELLO", out, quit);
    CHECK(out.str() == "002*second\n");
}

TEST_CASE("shell LIST file shows sorted record names") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-FILE BP", out, quit);
    sh.handleLine("WRITE BP ZREC z", out, quit);
    sh.handleLine("WRITE BP AREC a", out, quit);

    out.str("");
    sh.handleLine("LIST BP", out, quit);
    CHECK(out.str() == "Records:\n  AREC\n  ZREC\n");
}

TEST_CASE("shell LIST file empty and arity") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-FILE BP", out, quit);

    out.str("");
    sh.handleLine("LIST BP", out, quit);
    CHECK(out.str() == "No records\n");

    out.str("");
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "LIST requires a filename\n");
}

TEST_CASE("shell filesystem command arity and missing record") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-FILE", out, quit);
    CHECK(out.str() == "CREATE-FILE requires a filename\n");

    out.str("");
    sh.handleLine("DELETE-FILE", out, quit);
    CHECK(out.str() == "DELETE-FILE requires a filename\n");

    out.str("");
    sh.handleLine("LIST-FILES extra", out, quit);
    CHECK(out.str() == "LIST-FILES takes no arguments\n");

    out.str("");
    sh.handleLine("READ BP", out, quit);
    CHECK(out.str() == "READ requires a file and record name\n");

    out.str("");
    sh.handleLine("WRITE BP HELLO", out, quit);
    CHECK(out.str() == "WRITE requires a file, record name, and value\n");
}

TEST_CASE("shell filesystem missing file and missing record") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("READ BP HELLO", out, quit);
    CHECK(out.str().find("Error: File not found: BP") != std::string::npos);

    out.str("");
    sh.handleLine("LIST BP", out, quit);
    CHECK(out.str().find("Error: File not found: BP") != std::string::npos);

    out.str("");
    sh.handleLine("CREATE-FILE BP", out, quit);
    sh.handleLine("READ BP HELLO", out, quit);
    CHECK(out.str() == "No such record\n");
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

TEST_CASE("shell ASM mode owns VM debugger commands") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("STEP", out, quit);
    CHECK(out.str() == "Unknown command: STEP\n");

    out.str("");
    sh.handleLine("ASM", out, quit);
    sh.handleLine("STEP", out, quit);
    CHECK(out.str() == "No program loaded\n");

    out.str("");
    sh.handleLine("QUIT", out, quit);
    CHECK_FALSE(quit);

    out.str("");
    sh.handleLine("STEP", out, quit);
    CHECK(out.str() == "Unknown command: STEP\n");
}

TEST_CASE("shell STEP DUMP-PROGRAM DUMP-LABELS without loaded program") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("ASM", out, quit);
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
    sh.handleLine("ASM", out, quit);
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
    sh.handleLine("ASM", out, quit);
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
    sh.handleLine("ASM", out, quit);
    sh.handleLine("BREAKPOINT 5", out, quit);
    sh.handleLine("RUN tiny", out, quit);
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
    sh.handleLine("ASM", out, quit);
    sh.handleLine("TRACE ON", out, quit);
    sh.handleLine("RUN t", out, quit);
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
    sh.handleLine("ASM", out, quit);
    sh.handleLine("RUN lbl", out, quit);
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
    sh.handleLine("ASM", out, quit);
    sh.handleLine("BREAKPOINT 2", out, quit);
    sh.handleLine("RUN bp", out, quit);
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

TEST_CASE("shell BREAKPOINT index behavior is unchanged with source metadata present") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "bp_meta.tbc");
        f << "PUSH_INT 10\nPUSH_INT 20\nADD\nHALT\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("ASM", out, quit);

    sh.handleLine("BREAKPOINT 2", out, quit);
    sh.handleLine("RUN bp_meta", out, quit);
    CHECK(out.str().find("Breakpoint hit at 2") != std::string::npos);

    out.str("");
    sh.handleLine("STEP", out, quit);
    // Parser attaches .tbc physical line metadata; index-oriented STEP remains intact.
    CHECK(out.str().find("2: ADD") != std::string::npos);
    CHECK(out.str().find("(line 3)") != std::string::npos);
}

TEST_CASE("shell BASIC sample session edit flow") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC HELLO", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().empty());

    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    sh.handleLine("20 GOTO 10", out, quit);
    out.str("");

    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 PRINT \"HELLO\"\n20 GOTO 10\n");

    out.str("");
    sh.handleLine("EDIT 10", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("C/HELLO/WORLD/", out, quit);
    sh.handleLine("FI", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 PRINT \"WORLD\"\n20 GOTO 10\n");

    out.str("");
    sh.handleLine("RENUM", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 PRINT \"WORLD\"\n20 GOTO 10\n");

    out.str("");
    sh.handleLine("QUIT", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "LIST requires a filename\n");
}

TEST_CASE("shell BASIC SAVE requires name when unnamed") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");

    sh.handleLine("SAVE", out, quit);
    CHECK(out.str() == "No program name\n");
}

TEST_CASE("shell BASIC COMPILE and RUN require name when unnamed") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);

    out.str("");
    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "No program name\n");

    out.str("");
    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "No program name\n");
}

TEST_CASE("shell BASIC SAVE with explicit name persists and autoloads") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");
    sh.handleLine("SAVE HELLO", out, quit);
    CHECK(out.str().empty());
    CHECK(std::filesystem::exists(dir / "HELLO"));

    sh.handleLine("QUIT", out, quit);
    out.str("");
    sh.handleLine("BASIC HELLO", out, quit);
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 PRINT \"HELLO\"\n");
}

TEST_CASE("shell BASIC SAVE writes source only and not bytecode") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");
    sh.handleLine("SAVE HELLO", out, quit);
    CHECK(out.str().empty());
    CHECK(std::filesystem::exists(dir / "HELLO"));
    CHECK_FALSE(std::filesystem::exists(dir / "HELLO.tbc"));
}

TEST_CASE("shell BASIC COMPILE writes bytecode and failed COMPILE preserves prior bytecode") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC HELLO", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");
    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Compiled successfully.\nInstructions: 4\nLabels: 0\n");
    CHECK(std::filesystem::exists(dir / "HELLO.tbc"));

    const std::filesystem::path tbcPath = dir / "HELLO.tbc";
    const auto before = std::filesystem::last_write_time(tbcPath);

    sh.handleLine("20 MAT A = 1", out, quit);
    out.str("");
    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Error on line 20: Unknown keyword 'MAT'\nCompilation failed.\n");
    CHECK(std::filesystem::exists(tbcPath));
    const auto after = std::filesystem::last_write_time(tbcPath);
    CHECK(before == after);
}

TEST_CASE("shell BASIC named entry missing file starts empty and creates on SAVE only") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC HELLO", out, quit);
    CHECK_FALSE(std::filesystem::exists(dir / "HELLO"));
    out.str("");

    sh.handleLine("LIST", out, quit);
    CHECK(out.str().empty());
    CHECK_FALSE(std::filesystem::exists(dir / "HELLO"));

    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");
    sh.handleLine("SAVE", out, quit);
    CHECK(out.str().empty());
    CHECK(std::filesystem::exists(dir / "HELLO"));
}

TEST_CASE("shell BASIC EDIT requires existing line") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    out.str("");
    sh.handleLine("EDIT 10", out, quit);
    CHECK(out.str() == "No such line: 10\n");
}

TEST_CASE("shell BASIC DELETE supports line and ranges") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 A", out, quit);
    sh.handleLine("20 B", out, quit);
    sh.handleLine("30 C", out, quit);
    sh.handleLine("40 D", out, quit);

    out.str("");
    sh.handleLine("DELETE 20-30", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 A\n40 D\n");

    out.str("");
    sh.handleLine("DELETE 10", out, quit);
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "40 D\n");
}

TEST_CASE("shell BASIC DELETE rejects malformed ranges") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 A", out, quit);
    sh.handleLine("20 B", out, quit);

    out.str("");
    sh.handleLine("DELETE 20-10", out, quit);
    CHECK(out.str() == "DELETE requires a line number or range\n");

    out.str("");
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 A\n20 B\n");
}

TEST_CASE("shell BASIC SAVE and BASIC command arity errors") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC A B", out, quit);
    CHECK(out.str() == "BASIC takes at most one program name\n");

    out.str("");
    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("SAVE A B", out, quit);
    CHECK(out.str() == "SAVE takes at most one filename\n");
}

TEST_CASE("shell BASIC LOAD behavior and arity") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    out.str("");
    sh.handleLine("LOAD", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("LOAD A B", out, quit);
    CHECK(out.str() == "LOAD takes at most one filename\n");

    out.str("");
    sh.handleLine("10 PRINT \"TMP\"", out, quit);
    sh.handleLine("LOAD MISSING", out, quit);
    sh.handleLine("LIST", out, quit);
    CHECK(out.str().empty());
}

TEST_CASE("shell BASIC LOAD loads saved program") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    sh.handleLine("SAVE HELLO", out, quit);
    sh.handleLine("NEW", out, quit);

    out.str("");
    sh.handleLine("LOAD HELLO", out, quit);
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 PRINT \"HELLO\"\n");
}

TEST_CASE("shell BASIC COMPILE reports success summary") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 LET A = 5", out, quit);
    sh.handleLine("20 PRINT A", out, quit);
    sh.handleLine("30 END", out, quit);
    out.str("");

    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Compiled successfully.\nInstructions: 6\nLabels: 0\n");
}

TEST_CASE("shell BASIC RUN is quiet on compile success and executes output") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    sh.handleLine("20 END", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "HELLO\n");
}

TEST_CASE("shell BASIC RUN (C mirrors COMPILE and does not execute") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");

    sh.handleLine("RUN (C", out, quit);
    CHECK(out.str() == "Compiled successfully.\nInstructions: 4\nLabels: 0\n");
}

TEST_CASE("shell BASIC compile failures print line diagnostics") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("30 MAT A = 1", out, quit);
    out.str("");

    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Error on line 30: Unknown keyword 'MAT'\nCompilation failed.\n");
}

TEST_CASE("shell BASIC RUN compile failure skips execution") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 MAT A = 1", out, quit);
    sh.handleLine("20 PRINT \"SHOULD_NOT_RUN\"", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "Error on line 10: Unknown keyword 'MAT'\nCompilation failed.\n");
}

TEST_CASE("shell BASIC RUN recompiles each time and does not depend on COMPILE cache") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"ONE\"", out, quit);
    out.str("");
    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Compiled successfully.\nInstructions: 4\nLabels: 0\n");

    sh.handleLine("10 PRINT \"TWO\"", out, quit);
    out.str("");
    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "TWO\n");
}

TEST_CASE("shell BASIC RUN executes arithmetic expressions") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 LET A = 2+3*4", out, quit);
    sh.handleLine("20 PRINT A", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "14\n");
}

TEST_CASE("shell BASIC RUN executes INPUT then PRINT with injected input") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::istringstream in("123\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 INPUT A", out, quit);
    sh.handleLine("20 PRINT A", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "123\n");
}

TEST_CASE("shell BASIC RUN supports PRINT semicolon prompt before INPUT") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::istringstream in("123\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"Enter value: \";", out, quit);
    sh.handleLine("20 INPUT A", out, quit);
    sh.handleLine("30 PRINT A", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "Enter value: 123\n");
}

TEST_CASE("shell BASIC COMPILE reports expression syntax errors") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 LET A = (2+3", out, quit);
    out.str("");

    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Error on line 10: LET expression error: Missing ')'\nCompilation failed.\n");
}

TEST_CASE("shell BASIC COMPILE reports malformed INPUT") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 INPUT", out, quit);
    out.str("");

    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Error on line 10: INPUT requires a variable name\nCompilation failed.\n");
}

TEST_CASE("shell BASIC RUN executes IF THEN ELSE and GOTO control flow") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 LET A = 1", out, quit);
    sh.handleLine("20 IF A = 1 THEN 40 ELSE 30", out, quit);
    sh.handleLine("30 PRINT 0", out, quit);
    sh.handleLine("40 PRINT 1", out, quit);
    sh.handleLine("50 GOTO 70", out, quit);
    sh.handleLine("60 PRINT 2", out, quit);
    sh.handleLine("70 STOP", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "1\n");
}

TEST_CASE("shell BASIC RUN executes IF THEN ELSE inline statements") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 LET A = 1", out, quit);
    sh.handleLine("20 IF A = 1 THEN PRINT 111 ELSE PRINT 222", out, quit);
    sh.handleLine("30 IF A = 0 THEN PRINT 333 ELSE PRINT 444", out, quit);
    sh.handleLine("40 END", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "111\n444\n");
}

TEST_CASE("shell BASIC COMPILE reports unknown GOTO target line") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 GOTO 99", out, quit);
    out.str("");

    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Error on line 10: Unknown target line 99\nCompilation failed.\n");
}

TEST_CASE("shell BASIC RENUMBER aliases RENUM") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("100 A", out, quit);
    sh.handleLine("350 B", out, quit);

    out.str("");
    sh.handleLine("RENUMBER", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 A\n20 B\n");
}

TEST_CASE("shell BASIC RENUM rewrites GOTO targets") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("100 PRINT 1", out, quit);
    sh.handleLine("200 GOTO 100", out, quit);

    out.str("");
    sh.handleLine("RENUM", out, quit);
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 PRINT 1\n20 GOTO 10\n");
}

TEST_CASE("shell BASIC RENUM rewrites IF THEN ELSE targets") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("100 LET A = 1", out, quit);
    sh.handleLine("200 IF A = 1 THEN 400 ELSE 300", out, quit);
    sh.handleLine("300 PRINT 0", out, quit);
    sh.handleLine("400 PRINT 1", out, quit);

    out.str("");
    sh.handleLine("RENUM", out, quit);
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 LET A = 1\n20 IF A = 1 THEN 40 ELSE 30\n30 PRINT 0\n40 PRINT 1\n");
}

TEST_CASE("shell BASIC RENUM leaves dangling targets unchanged") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("100 GOTO 999", out, quit);
    sh.handleLine("200 IF 1 = 1 THEN 999 ELSE 100", out, quit);

    out.str("");
    sh.handleLine("RENUM", out, quit);
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 GOTO 999\n20 IF 1 = 1 THEN 999 ELSE 10\n");
}

TEST_CASE("shell ED mode malformed substitute command is rejected") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    sh.handleLine("EDIT 10", out, quit);

    out.str("");
    sh.handleLine("C/HELLO/WORLD/ extra", out, quit);
    CHECK(out.str() == "Invalid edit command\n");

    out.str("");
    sh.handleLine("C/HELLO/WORLD/", out, quit);
    sh.handleLine("FI", out, quit);
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 PRINT \"WORLD\"\n");
}

TEST_CASE("shell BASIC RUN reports runtime error for RETURN without GOSUB") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::istringstream in("QUIT\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"before\";", out, quit);
    sh.handleLine("20 RETURN", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str().find("Runtime error") != std::string::npos);
    CHECK(out.str().find("RETURN without GOSUB") != std::string::npos);
    // Shell must remain usable after a runtime error — BASIC prompt should still work
    CHECK_FALSE(quit);
}

TEST_CASE("shell BASIC RUN reports runtime error for divide by zero") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::istringstream in("QUIT\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT 1/0", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str().find("Runtime error") != std::string::npos);
    CHECK(out.str().find("divide by zero") != std::string::npos);
}

TEST_CASE("shell BASIC debugger supports LIST and QUIT after runtime error") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::istringstream in("LIST\nQUIT\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"before\";", out, quit);
    sh.handleLine("20 RETURN", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str().find("Runtime error: RETURN without GOSUB") != std::string::npos);
    CHECK(out.str().find("* ") != std::string::npos);
    CHECK(out.str().find("10 PRINT \"before\";") != std::string::npos);
}

TEST_CASE("shell BASIC debugger supports breakpoint and CONT by BASIC line") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::istringstream in("BREAKPOINT 20\nCONT\nQUIT\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 LET A = 1", out, quit);
    sh.handleLine("20 GOTO 10", out, quit);
    out.str("");

    std::thread interrupter([&rt]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        rt.interrupt();
    });
    sh.handleLine("RUN", out, quit);
    interrupter.join();
    CHECK(out.str().find("Break") != std::string::npos);
    CHECK(out.str().find("Breakpoint hit at line 20") != std::string::npos);
}

TEST_CASE("shell END exits vm debugger context") {
    auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "bp_end.tbc");
        f << "PUSH_INT 10\nPUSH_INT 20\nADD\nHALT\n";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("ASM", out, quit);

    sh.handleLine("BREAKPOINT 2", out, quit);
    sh.handleLine("RUN bp_end", out, quit);
    CHECK(out.str().find("Breakpoint hit at 2") != std::string::npos);

    out.str("");
    sh.handleLine("END", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "RUN requires a filename\n");
}

TEST_CASE("shell BASIC RUN executes OPEN WRITE READ CLOSE flow") {
    auto progDir = uniqueTempDir();
    auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(progDir);
    std::filesystem::create_directories(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(progDir);
    sh.setFileSystemRoot(fsDir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-FILE BP", out, quit);
    CHECK(out.str().empty());
    out.str("");

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 OPEN \"BP\" TO F ELSE 90", out, quit);
    sh.handleLine("20 WRITE \"HELLO\" ON F, \"ID1\" ELSE 90", out, quit);
    sh.handleLine("30 READ R FROM F, \"ID1\" ELSE 90", out, quit);
    sh.handleLine("40 PRINT R", out, quit);
    sh.handleLine("50 CLOSE F", out, quit);
    sh.handleLine("60 END", out, quit);
    sh.handleLine("90 PRINT \"ERR\"", out, quit);
    sh.handleLine("100 END", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "HELLO\n");
}

TEST_CASE("shell BASIC RUN executes OPEN ELSE inline statement on missing file") {
    auto progDir = uniqueTempDir();
    auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(progDir);
    std::filesystem::create_directories(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setProgramsRoot(progDir);
    sh.setFileSystemRoot(fsDir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 OPEN \"MISSING\" TO F ELSE PRINT \"MISS\"", out, quit);
    sh.handleLine("20 PRINT \"AFTER\"", out, quit);
    sh.handleLine("30 END", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "MISS\nAFTER\n");
}
