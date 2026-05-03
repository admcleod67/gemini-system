#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "FileSystem.h"
#include "Runtime.h"
#include "Shell.h"
#include "ShellTestFsUtil.h"

using PickTests::seedVocAndProc;
using PickTests::uniqueTempDir;
using PickTests::writeProcScriptRecord;
using PickTests::writeRecord;

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
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC MISSING", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Error: Cannot open PROC script: MISSING\n");
}

TEST_CASE("shell PROC assignment display and token substitution") {
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    writeProcScriptRecord(fsDir, "FLOW", "MSG = NOOP\nX = HELLO\nDISPLAY X WORLD\nDISPLAY MSG\nEND\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC FLOW", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "HELLO WORLD\nNOOP\n");
}

TEST_CASE("shell PROC INPUT stores value and can be displayed") {
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    writeProcScriptRecord(fsDir, "ASK", "INPUT USER\nDISPLAY USER\nEND\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::istringstream in("ALICE\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC ASK", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "ALICE\n");
}

TEST_CASE("shell PROC GO supports forward and backward labels") {
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    writeProcScriptRecord(fsDir, "JUMP", "GO NEXT\nDISPLAY BAD\nNEXT:\nDISPLAY OK\nEND\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC JUMP", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "OK\n");
}

TEST_CASE("shell PROC IF THEN GO with case-insensitive labels") {
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    writeProcScriptRecord(fsDir, "IFLOW", "X = YES\nIF X = YES THEN GO menu\nDISPLAY FAIL\nMENU:\nDISPLAY PASS\nEND\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC IFLOW", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "PASS\n");
}

TEST_CASE("shell PROC positional parameters substitute into TCL command") {
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    writeProcScriptRecord(fsDir, "BRIDGE", "TCL ECHO P1 P2\nEND\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC BRIDGE HELLO WORLD", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "HELLO WORLD\n");
}

TEST_CASE("shell PROC duplicate labels are rejected") {
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    writeProcScriptRecord(fsDir, "DUP", "A:\nA:\nEND\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC DUP", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Error: Duplicate label: A\n");
}

TEST_CASE("shell PROC malformed IF and unknown label errors") {
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    writeProcScriptRecord(fsDir, "BADIF", "IF A = B THEN DISPLAY X\nEND\n");
    writeProcScriptRecord(fsDir, "BADGO", "GO MISSING\nEND\n");

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
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

TEST_CASE("shell PROC uses F mapping for script lookup outside PROC file") {
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    PickFS::FileSystem fs(fsDir);
    fs.createFile("SCRIPTS");
    fs.write("VOC", PickFS::Record("FLOW", "F\nSCRIPTS\n"));
    fs.write("SCRIPTS", PickFS::Record("FLOW", "DISPLAY OK\nEND\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC FLOW", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "OK\n");
}

TEST_CASE("shell PROC uses Q chain for script lookup") {
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    PickFS::FileSystem fs(fsDir);
    fs.write("VOC", PickFS::Record("FLOW", "Q\nREALFLOW\n"));
    fs.write("VOC", PickFS::Record("REALFLOW", "F\nPROC\n"));
    fs.write("PROC", PickFS::Record("FLOW", "DISPLAY QOK\nEND\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC FLOW", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "QOK\n");
}

TEST_CASE("shell PROC bridge resolves V-type verb aliases") {
    auto fsDir = uniqueTempDir();
    seedVocAndProc(fsDir);
    PickFS::FileSystem fs(fsDir);
    fs.write("VOC", PickFS::Record("SAYWHO", "V\nWHO\n"));
    fs.write("PROC", PickFS::Record("SHOW", "TCL SAYWHO\nEND\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("PROC SHOW", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "0 - -\n");
}

TEST_CASE("shell PROC TCL EDIT blocks until editor QUIT before next PROC line") {
    auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    seedVocAndProc(fsDir);
    PickFS::FileSystem fs(fsDir);
    fs.createFile("BP");
    writeRecord(fsDir, "BP", "EDTMP", "");

    const std::string procScript = R"(DISPLAY before
TCL EDIT BP EDTMP
DISPLAY after
END
)";
    writeProcScriptRecord(fsDir, "EDPROC", procScript);

    std::istringstream in("I\npatched\n.\nFI\nQ\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("PROC EDPROC", out, quit);
    CHECK(out.str().find("before") != std::string::npos);
    CHECK(out.str().find("after") != std::string::npos);
    CHECK(out.str().find("before") < out.str().find("after"));

    out.str("");
    sh.handleLine("READ BP EDTMP", out, quit);
    CHECK(out.str() == "patched\n");
}
