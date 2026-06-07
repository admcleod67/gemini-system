#include <doctest/doctest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include <pick_system/version.hpp>

#include "FileSystem.h"
#include "LoginService.h"
#include "HelpTopics.h"
#include "Runtime.h"
#include "Shell.h"
#include "ShellTestFsUtil.h"

using PickTests::uniqueTempDir;
using PickTests::writeProcScriptRecord;
using PickTests::writeRecord;

static void seedVocAndBp(const std::filesystem::path &fsRoot) {
    PickFS::FileSystem fs(fsRoot);
    fs.createFile("VOC");
    fs.createFile("BP");
    fs.write("VOC", PickFS::Record("BP", "001 F\n002 BP\n003 /gemini/fs/DM/BP\n"));
}

static void writeProgramSourceRecord(const std::filesystem::path &fsRoot,
                                     const std::string &programName,
                                     const std::string &source) {
    writeRecord(fsRoot, "BP", programName, source);
}

static void writeProgramObjectRecord(const std::filesystem::path &fsRoot,
                                     const std::string &programName,
                                     const std::string &objectText) {
    writeRecord(fsRoot, "BP", programName + "_OBJ", objectText);
}

TEST_CASE("shell HELP") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == PickShell::HelpTopics::helpZeroArgumentFallbackLine());
    CHECK(out.str().find("HELP-LIST") != std::string::npos);
}

TEST_CASE("shell HELP supports command lookup and topics") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("HELP GET", out, quit);
    CHECK(out.str() == "GET <name>\n");

    out.str("");
    sh.handleLine("HELP PROC", out, quit);
    CHECK(out.str().find("HELP PROC") != std::string::npos);
    CHECK(out.str().find("PROC <name> [args...]") != std::string::npos);

    out.str("");
    sh.handleLine("HELP TCL", out, quit);
    CHECK(out.str().find("HELP TCL") != std::string::npos);

    out.str("");
    sh.handleLine("HELP VOC", out, quit);
    CHECK(out.str().find("CREATE-VOC") != std::string::npos);

    out.str("");
    sh.handleLine("HELP UNKNOWN", out, quit);
    CHECK(out.str() == "No help available for \"UNKNOWN\".\n");
}

TEST_CASE("shell HELP precedence and non-executing lookup via VOC alias") {
    const auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(fsDir);
    PickFS::FileSystem fs(fsDir);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("HGET", "V\nGET\n"));
    fs.write("VOC", PickFS::Record("HVOC", "V\nVOC\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("HELP HGET", out, quit);
    CHECK(out.str() == "GET <name>\n");

    out.str("");
    sh.handleLine("HELP HVOC", out, quit);
    CHECK(out.str().find("HELP VOC") != std::string::npos);
    CHECK(out.str().find("LIST-VOC") != std::string::npos);
}

TEST_CASE("shell HELP zero-arg uses HELP topic record when present") {
    const auto fsDir = uniqueTempDir();
    PickFS::FileSystem fs(fsDir);
    fs.createFile("HELP");
    fs.write("HELP", PickFS::Record("HELP", "zero arg body\n"));
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP", out, quit);
    CHECK(out.str() == "zero arg body\n");
}

TEST_CASE("shell HELP multi-word topic maps to underscore record id and HELP-LIST shows canonical id") {
    const auto fsDir = uniqueTempDir();
    PickFS::FileSystem fs(fsDir);
    fs.createFile("HELP");
    fs.write("HELP", PickFS::Record("HELP_BASIC", "multiword body\n"));
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP HELP BASIC", out, quit);
    CHECK(out.str() == "multiword body\n");
    CHECK(std::filesystem::exists(fsDir / "HELP" / "HELP_BASIC.item"));
    out.str("");
    sh.handleLine("HELP-LIST", out, quit);
    CHECK(out.str() == "HELP BASIC\n");
}

TEST_CASE("shell HELP COMMANDS lists Tcl verbs with intro") {
    const auto fsDir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP COMMANDS", out, quit);
    CHECK(out.str().find("HELP COMMANDS lists the TCL command verbs") != std::string::npos);
    CHECK(out.str().find("HELP <command>") != std::string::npos);
    CHECK(out.str().find("\nGET\n") != std::string::npos);
    CHECK(out.str().find("\nWHO\n") != std::string::npos);
}

TEST_CASE("shell HELP HELP COMMANDS matches HELP COMMANDS") {
    const auto fsDir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream a;
    std::ostringstream b;
    bool qa = false;
    bool qb = false;
    sh.handleLine("HELP HELP COMMANDS", a, qa);
    sh.handleLine("HELP COMMANDS", b, qb);
    CHECK_FALSE(qa);
    CHECK_FALSE(qb);
    CHECK(a.str() == b.str());
}

TEST_CASE("shell HELP COMMANDS includes VOC aliases that dispatch to Tcl verbs") {
    const auto fsDir = uniqueTempDir();
    PickFS::FileSystem fs(fsDir);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("ZALT", "V\nBASIC\n"));
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP COMMANDS", out, quit);
    CHECK(out.str().find("\nZALT\n") != std::string::npos);
    CHECK(out.str().find("\nBASIC\n") != std::string::npos);
}

TEST_CASE("shell HELP COMMANDS file record overrides dynamic listing") {
    const auto fsDir = uniqueTempDir();
    PickFS::FileSystem fs(fsDir);
    fs.createFile("HELP");
    fs.write("HELP", PickFS::Record("HELP_COMMANDS", "static override topic\n"));
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP COMMANDS", out, quit);
    CHECK(out.str() == "static override topic\n");
}

TEST_CASE("shell HELP resolves topic from SYSPROG when local account has no HELP file") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    std::filesystem::create_directories(gem / "accounts" / "TST" / "VOC");
    std::filesystem::create_directories(gem / "accounts" / "SYSPROG" / "VOC");
    std::filesystem::create_directories(gem / "accounts" / "SYSPROG" / "HELP");
    {
        std::ofstream vocTst(gem / "accounts" / "TST" / "VOC" / "BP.item");
        vocTst << "F\nBP\n";
    }
    {
        std::ofstream vocSys(gem / "accounts" / "SYSPROG" / "VOC" / "BP.item");
        vocSys << "F\nBP\n";
    }
    {
        std::ofstream h(gem / "accounts" / "SYSPROG" / "HELP" / "REMOTEONLY.item");
        h << "from system help\n";
    }
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"},{"name":"SYSPROG","root":"accounts/SYSPROG"}]})";
    }
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setGeminiCatalogRoot(gem);
    std::ostringstream err;
    const auto session = PickCore::LoginService::authenticateAccount(gem, "TST", "", err);
    REQUIRE(session.has_value());
    sh.attachUserSession(*session);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP REMOTEONLY", out, quit);
    CHECK(out.str() == "from system help\n");
}

TEST_CASE("shell HELP-LIST reports no topics when HELP file is missing") {
    const auto fsDir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP-LIST", out, quit);
    CHECK(out.str() == "No HELP topics\n");
}

#ifndef PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE
#define PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE ""
#endif

TEST_CASE("shell HELP HELP BASIC maps to committed SYSPROG HELP_BASIC.item") {
    if (std::string(PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE).empty()) {
        return;
    }
    const std::filesystem::path sp(PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE);
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(sp);
    REQUIRE(std::filesystem::exists(sp / "HELP" / "HELP_BASIC.item"));
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP HELP BASIC", out, quit);
    CHECK(out.str().find("underscore") != std::string::npos);
}

TEST_CASE("shell WHO returns default port user account line") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("WHO", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "0 - -\n");
}

TEST_CASE("shell top-level Tcl resolves VOC verb alias SAYWHO to WHO") {
    const auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(fsDir);
    PickFS::FileSystem fs(fsDir);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("SAYWHO", "V\nWHO\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("SAYWHO", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "0 - -\n");
    out.str("");
    sh.handleLine("saywho", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "0 - -\n");
}

TEST_CASE("shell top-level Tcl resolves VOC ED alias to EDIT") {
    const auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(fsDir);
    PickFS::FileSystem fs(fsDir);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("ED", "V\nEDIT\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("ED", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("EDIT requires") != std::string::npos);
    out.str("");
    sh.handleLine("ed", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("EDIT requires") != std::string::npos);
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

TEST_CASE("shell ECHO supports quoted tokens and escapes") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("ECHO \"hello world\" x", out, quit);
    CHECK(out.str() == "hello world x\n");

    out.str("");
    sh.handleLine("ECHO \"\"", out, quit);
    CHECK(out.str() == "\n");

    out.str("");
    sh.handleLine("ECHO \"\" x", out, quit);
    CHECK(out.str() == " x\n");

    out.str("");
    sh.handleLine("ECHO \"a\\\"b\" c\\\\d", out, quit);
    CHECK(out.str() == "a\"b c\\d\n");
}

TEST_CASE("shell tokenizer errors on malformed quote and escape") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("ECHO \"unterminated", out, quit);
    CHECK(out.str() == "Error: Unterminated quoted string\n");

    out.str("");
    sh.handleLine("ECHO trailing\\", out, quit);
    CHECK(out.str() == "Error: Invalid escape sequence\n");
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

    out.str("");
    sh.handleLine("GET nope extra", out, quit);
    CHECK(out.str() == "GET requires a variable name\n");
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

    out.str("");
    sh.handleLine("UNSET x extra", out, quit);
    CHECK(out.str() == "UNSET requires a variable name\n");
}

TEST_CASE("shell strict no-arg command arity checks") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("HELP extra", out, quit);
    CHECK(out.str() == "No help available for \"extra\".\n");

    out.str("");
    sh.handleLine("HELP A B", out, quit);
    CHECK(out.str() == "No help available for \"A B\".\n");

    out.str("");
    sh.handleLine("VERSION extra", out, quit);
    CHECK(out.str() == "VERSION takes no arguments\n");

    out.str("");
    sh.handleLine("WHO extra", out, quit);
    CHECK(out.str() == "WHO takes no arguments\n");

    out.str("");
    sh.handleLine("DUMP-STACK extra", out, quit);
    CHECK(out.str() == "DUMP-STACK takes no arguments\n");

    out.str("");
    sh.handleLine("LIST-PROGRAMS extra", out, quit);
    CHECK(out.str() == "LIST-PROGRAMS takes no arguments\n");

    out.str("");
    sh.handleLine("LOGOFF extra", out, quit);
    CHECK(out.str() == "LOGOFF takes no arguments\n");

    out.str("");
    sh.handleLine("SYSTEM extra", out, quit);
    CHECK(out.str() == "SYSTEM takes no arguments\n");

    out.str("");
    sh.handleLine("ABOUT extra", out, quit);
    CHECK(out.str() == "ABOUT takes no arguments\n");
}

TEST_CASE("shell SYSTEM and ABOUT introspection output") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("SYSTEM", out, quit);
    CHECK(out.str().find("System:") != std::string::npos);
    CHECK(out.str().find("Version:") != std::string::npos);
    CHECK(out.str().find("Build:") != std::string::npos);
    CHECK(out.str().find("Session: 0 - -") != std::string::npos);
    const std::string systemOutput = out.str();

    out.str("");
    sh.handleLine("ABOUT", out, quit);
    CHECK(out.str() == systemOutput);
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
    const auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("RUN missing", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("Error: Cannot open bytecode file for program:") != std::string::npos);
}

TEST_CASE("shell RUN executes bytecode from programs root") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramObjectRecord(fsDir, "mini", "PUSH_INT 5\nHALT\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
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
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramSourceRecord(fsDir, "HELLO", "10 PRINT \"HI\"\n20 END\n");

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;

    PickFS::FileSystem fs(fsDir);
    CHECK_FALSE(fs.read("BP", "HELLO_OBJ").has_value());
    sh.handleLine("RUN HELLO", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "HI\n");
    CHECK(fs.read("BP", "HELLO_OBJ").has_value());
}

TEST_CASE("shell RUN compile failure from BASIC source reports diagnostics") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramSourceRecord(fsDir, "BROKEN", "10 ZAP A = 1\n");

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("RUN BROKEN", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Error on line 10: Unknown keyword 'ZAP'\nCompilation failed.\n");
    PickFS::FileSystem fs(fsDir);
    CHECK_FALSE(fs.read("BP", "BROKEN_OBJ").has_value());
}

TEST_CASE("shell LIST-PROGRAMS empty directory") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Programs:\n");
}

TEST_CASE("shell LIST-PROGRAMS lists logical names from tbc and bas") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramObjectRecord(fsDir, "a", "HALT\n");
    writeProgramSourceRecord(fsDir, "a", "10 END\n");
    writeProgramSourceRecord(fsDir, "b", "10 END\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Programs:\n  a\n  b\n");
}

TEST_CASE("shell LIST-PROGRAMS deduplicates bas and tbc with same basename") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramSourceRecord(fsDir, "HELLO", "10 PRINT \"HI\"\n");
    writeProgramObjectRecord(fsDir, "HELLO", "HALT\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Programs:\n  HELLO\n");
}

TEST_CASE("shell LIST-PROGRAMS matches extension case-insensitively and sorts names") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramObjectRecord(fsDir, "zeta", "HALT\n");
    writeProgramSourceRecord(fsDir, "zeta", "10 END\n");
    writeProgramSourceRecord(fsDir, "alpha", "10 END\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Programs:\n  alpha\n  zeta\n");
}

TEST_CASE("shell LIST-PROGRAMS missing directory") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(uniqueTempDir());
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LIST-PROGRAMS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "Programs:\n");
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
    CHECK(std::filesystem::is_directory(dir / "BP"));

    out.str("");
    sh.handleLine("LIST-FILES", out, quit);
    CHECK(out.str() == "Files:\n  BP\n");

    out.str("");
    sh.handleLine("DELETE-FILE BP", out, quit);
    CHECK(out.str().empty());
    CHECK_FALSE(std::filesystem::exists(dir / "BP"));

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
    CHECK(std::filesystem::exists(dir / "BP" / "HELLO.bas"));

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

TEST_CASE("shell LIST with fields routes to ENGLISH while LIST file stays legacy") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    fs.write("DATA", PickFS::Record("R1", "ALICE"));
    fs.write("DATA", PickFS::Record("R2", "BOB"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    out.str("");
    sh.handleLine("LIST DATA", out, quit);
    CHECK(out.str() == "Records:\n  R1\n  R2\n");

    out.str("");
    sh.handleLine("LIST DATA NAME", out, quit);
    CHECK(out.str().find("R1 ALICE") != std::string::npos);
    CHECK(out.str().find("R2 BOB") != std::string::npos);
}

TEST_CASE("shell LIST with HEADING emits the heading as the first line") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    fs.write("DATA", PickFS::Record("R1", "ALICE"));
    fs.write("DATA", PickFS::Record("R2", "BOB"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("LIST DATA NAME HEADING \"Customer Report\"", out, quit);
    const std::string s = out.str();
    // First line should be the heading; the rows follow in some order.
    const auto firstNewline = s.find('\n');
    REQUIRE(firstNewline != std::string::npos);
    CHECK(s.substr(0, firstNewline) == "Customer Report");
    CHECK(s.find("R1 ALICE") != std::string::npos);
    CHECK(s.find("R2 BOB") != std::string::npos);
}

TEST_CASE("shell GET PAGE-LENGTH defaults to 24") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("GET PAGE-LENGTH", out, quit);
    CHECK(out.str() == "24\n");
}

TEST_CASE("shell SET PAGE-LENGTH validates a positive integer") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("SET PAGE-LENGTH 30", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("GET PAGE-LENGTH", out, quit);
    CHECK(out.str() == "30\n");

    out.str("");
    sh.handleLine("SET PAGE-LENGTH 0", out, quit);
    CHECK(out.str() == "SET PAGE-LENGTH requires a positive integer\n");

    out.str("");
    sh.handleLine("SET PAGE-LENGTH -5", out, quit);
    CHECK(out.str() == "SET PAGE-LENGTH requires a positive integer\n");

    out.str("");
    sh.handleLine("SET PAGE-LENGTH abc", out, quit);
    CHECK(out.str() == "SET PAGE-LENGTH requires a positive integer\n");

    out.str("");
    sh.handleLine("SET PAGE-LENGTH 12 extra", out, quit);
    CHECK(out.str() == "SET PAGE-LENGTH requires a positive integer\n");

    out.str("");
    sh.handleLine("SET PAGE-LENGTH", out, quit);
    CHECK(out.str() == "SET PAGE-LENGTH requires a positive integer\n");

    // Earlier successful SET 30 should still be in effect.
    out.str("");
    sh.handleLine("GET PAGE-LENGTH", out, quit);
    CHECK(out.str() == "30\n");
}

TEST_CASE("shell PAGE-LENGTH is not surfaced as a plain env variable") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("SET PAGE-LENGTH 17", out, quit);
    out.str("");
    sh.handleLine("LIST-VARS", out, quit);
    // PAGE-LENGTH is a typed system setting, not a user variable.
    CHECK(out.str().find("PAGE-LENGTH") == std::string::npos);
}

TEST_CASE("shell LIST with HEADING paginates per SET PAGE-LENGTH") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    for (int i = 1; i <= 12; ++i) {
        char id[8]{};
        std::snprintf(id, sizeof id, "R%02d", i);
        fs.write("DATA", PickFS::Record(id, "N"));
    }

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("SET PAGE-LENGTH 5", out, quit);
    out.str("");
    sh.handleLine("LIST DATA NAME HEADING \"Report\"", out, quit);

    // Layout for 12 records with pageLength=5:
    //   page 1 = heading + 4 rows                       (5 lines)
    //   blank + page 2 = blank + heading + 4 rows       (6 lines)
    //   blank + page 3 = blank + heading + 4 rows       (6 lines)
    // Total = 17 lines (each terminated by \n) -> 17 newlines.
    const std::string s = out.str();
    const std::size_t headingHits = [&]() {
        std::size_t n = 0;
        std::size_t pos = 0;
        while ((pos = s.find("Report", pos)) != std::string::npos) {
            ++n;
            pos += 6;
        }
        return n;
    }();
    CHECK(headingHits == 3);
    // Two blank-line separators inside the output (between pages).
    CHECK(s.find("\n\n") != std::string::npos);
    // All 12 record ids appear in the output.
    for (int i = 1; i <= 12; ++i) {
        char id[8]{};
        std::snprintf(id, sizeof id, "R%02d", i);
        CHECK(s.find(id) != std::string::npos);
    }
}

TEST_CASE("shell builtin HELP LIST SORT SELECT mention formatting clauses") {
    const auto fsDir = uniqueTempDir();
    PickFS::FileSystem fs(fsDir);
    const std::optional<std::string> listBody =
        PickShell::HelpTopics::resolveHelpBody(fs, std::nullopt, fsDir, "LIST");
    const std::optional<std::string> sortBody =
        PickShell::HelpTopics::resolveHelpBody(fs, std::nullopt, fsDir, "SORT");
    const std::optional<std::string> selectBody =
        PickShell::HelpTopics::resolveHelpBody(fs, std::nullopt, fsDir, "SELECT");
    REQUIRE(listBody.has_value());
    REQUIRE(sortBody.has_value());
    REQUIRE(selectBody.has_value());
    for (const std::string &body: {*listBody, *sortBody, *selectBody}) {
        CHECK(body.find("HEADING") != std::string::npos);
        CHECK(body.find("FOOTING") != std::string::npos);
        CHECK(body.find("BREAK-ON") != std::string::npos);
        CHECK(body.find("TOTAL") != std::string::npos);
        CHECK(body.find("ID-SUPP") != std::string::npos);
        CHECK(body.find("english-formatting.md") != std::string::npos);
    }
}

TEST_CASE("shell HELP LIST SORT SELECT mention formatting clauses") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    for (const char *verb: {"LIST", "SORT", "SELECT"}) {
        out.str("");
        sh.handleLine(std::string("HELP ") + verb, out, quit);
        const std::string s = out.str();
        CHECK(s.find("HEADING") != std::string::npos);
        CHECK(s.find("FOOTING") != std::string::npos);
        CHECK(s.find("BREAK-ON") != std::string::npos);
        CHECK(s.find("TOTAL") != std::string::npos);
        CHECK(s.find("ID-SUPP") != std::string::npos);
    }
}

TEST_CASE("shell LIST with FOOTING emits footer line") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    fs.write("DATA", PickFS::Record("R1", "ALICE\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("DEFINE-FIELD DICT NAME 1", out, quit);
    out.str("");
    sh.handleLine("LIST DATA NAME FOOTING \"End of report\"", out, quit);

    const std::string s = out.str();
    CHECK(s.find("ALICE") != std::string::npos);
    CHECK(s.find("End of report") != std::string::npos);
}

TEST_CASE("shell HELP LIST uses committed SYSPROG formatting help when fixture set") {
    if (std::string(PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE).empty()) {
        return;
    }
    const std::filesystem::path sp(PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE);
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(sp);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP LIST", out, quit);
    CHECK(out.str().find("ID-SUPP") != std::string::npos);
    CHECK(out.str().find("FOOTING") != std::string::npos);
    CHECK(out.str().find("english-formatting.md") != std::string::npos);
}

TEST_CASE("shell LIST with ID-SUPP omits record ids") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    fs.write("DATA", PickFS::Record("R1", "ALICE\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("DEFINE-FIELD DICT NAME 1", out, quit);
    out.str("");
    sh.handleLine("LIST DATA NAME ID-SUPP", out, quit);

    const std::string s = out.str();
    CHECK(s.find("ALICE") != std::string::npos);
    CHECK(s.find("R1") == std::string::npos);
}

TEST_CASE("shell LIST with TOTAL emits grand total line") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    fs.write("DICT", PickFS::Record("AMOUNT", "A\n2\n"));
    fs.write("DATA", PickFS::Record("R1", "ALICE\n100\n"));
    fs.write("DATA", PickFS::Record("R2", "BOB\n50\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("DEFINE-FIELD DICT NAME 1", out, quit);
    sh.handleLine("DEFINE-FIELD DICT AMOUNT 2", out, quit);
    out.str("");
    sh.handleLine("LIST DATA NAME AMOUNT TOTAL AMOUNT", out, quit);

    const std::string s = out.str();
    CHECK(s.find("TOTAL AMOUNT: 150") != std::string::npos);
    CHECK(s.find("ALICE") != std::string::npos);
    CHECK(s.find("BOB") != std::string::npos);
}

TEST_CASE("shell LIST with BREAK-ON emits full-width hyphen line on group change") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    fs.write("DICT", PickFS::Record("CITY", "A\n2\n"));
    fs.write("DATA", PickFS::Record("R1", "ALICE\nLONDON\n"));
    fs.write("DATA", PickFS::Record("R2", "BOB\nLONDON\n"));
    fs.write("DATA", PickFS::Record("R3", "CAROL\nPARIS\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("DEFINE-FIELD DICT NAME 1", out, quit);
    sh.handleLine("DEFINE-FIELD DICT CITY 2", out, quit);
    out.str("");
    sh.handleLine("LIST DATA NAME CITY BREAK-ON CITY", out, quit);

    const std::string s = out.str();
    const std::string hyphen(79, '-');
    CHECK(s.find(hyphen) != std::string::npos);
    const auto hyphenPos = s.find(hyphen);
    REQUIRE(hyphenPos != std::string::npos);
    CHECK(s.find("CAROL", hyphenPos) != std::string::npos);
    CHECK(s.find("ALICE") != std::string::npos);
    CHECK(s.find("BOB") != std::string::npos);
}

TEST_CASE("shell COUNT SELECT LIST-LIST CLEAR-LIST") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-FILE DATA", out, quit);
    sh.handleLine("WRITE DATA R1 A", out, quit);
    sh.handleLine("WRITE DATA R2 B", out, quit);

    out.str("");
    sh.handleLine("COUNT DATA", out, quit);
    CHECK(out.str() == "2\n");

    out.str("");
    sh.handleLine("SELECT DATA", out, quit);
    CHECK(out.str() == "2 records selected\n");

    out.str("");
    sh.handleLine("LIST-LIST", out, quit);
    CHECK(out.str() == "Active list:\n  R1\n  R2\n");

    out.str("");
    sh.handleLine("CLEAR-LIST", out, quit);
    CHECK(out.str() == "Active list cleared\n");

    out.str("");
    sh.handleLine("LIST-LIST", out, quit);
    CHECK(out.str() == "No active list\n");
}

TEST_CASE("shell SORT ENGLISH sorts and legacy SORT reserved message") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NM", "A\n1\n"));
    fs.write("DATA", PickFS::Record("RA", "Z\n"));
    fs.write("DATA", PickFS::Record("RB", "A\n"));

    out.str("");
    sh.handleLine("SORT DATA NM BY NM", out, quit);
    CHECK(out.str().find("RB A") != std::string::npos);
    CHECK(out.str().find("RA Z") != std::string::npos);

    sh.handleLine("CREATE-FILE NOPROJ", out, quit);
    sh.handleLine("WRITE NOPROJ RXX val", out, quit);
    out.str("");
    sh.handleLine("SORT NOPROJ", out, quit);
    CHECK(out.str() == "RXX\n");

    out.str("");
    sh.handleLine("SORT GARBAGE", out, quit);
    CHECK(out.str() == "SORT: ENGLISH syntax not detected; legacy Tcl SORT is not implemented\n");
}

TEST_CASE("shell RESOLVE-FIELD arity and DICT resolution") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NM", "A\n1\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("RESOLVE-FIELD", out, quit);
    CHECK(out.str().find("RESOLVE-FIELD takes") == 0);

    out.str("");
    sh.handleLine("RESOLVE-FIELD DATA NM", out, quit);
    CHECK(out.str().find("DICT-DATA") != std::string::npos);
    CHECK(out.str().find("Field kind: A") != std::string::npos);
    CHECK(out.str().find("Resolved attribute: 1") != std::string::npos);
    CHECK(out.str().find("Conversion hint") != std::string::npos);
}

TEST_CASE("shell RESOLVE-FIELD shows F-type DICT layout") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("THIRD", "F\n2\n3\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("RESOLVE-FIELD DATA THIRD", out, quit);
    const std::string s = out.str();
    CHECK(s.find("Field kind: F") != std::string::npos);
    CHECK(s.find("Source attribute: 2") != std::string::npos);
    CHECK(s.find("Selector: value 3") != std::string::npos);
    CHECK(s.find("Tail (raw): 3") != std::string::npos);
}

TEST_CASE("shell RESOLVE-FIELD shows OCONV on F-type conversion tail") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("PICKDAY", "F\n2\nD\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("RESOLVE-FIELD DATA PICKDAY", out, quit);
    const std::string s = out.str();
    CHECK(s.find("conversion OCONV \"D\"") != std::string::npos);
}

TEST_CASE("shell RESOLVE-FIELD shows I-type DICT layout") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NET", "I\nA + B\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("RESOLVE-FIELD DATA NET", out, quit);
    const std::string s = out.str();
    CHECK(s.find("Field kind: I") != std::string::npos);
    CHECK(s.find("Expression: A + B") != std::string::npos);
}

TEST_CASE("shell RESOLVE-FIELD shows invalid I-type DICT diagnostics") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("BAD", "I\n\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("RESOLVE-FIELD DATA BAD", out, quit);
    const std::string s = out.str();
    CHECK(s.find("Field kind: I") != std::string::npos);
    CHECK(s.find("Validity: INVALID") != std::string::npos);
    CHECK(s.find("Invalid I-type DICT item: missing expression") != std::string::npos);
    CHECK(s.find("Expression: (empty)") != std::string::npos);
}

TEST_CASE("shell LIST-DICT arity and missing dict file") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("LIST-DICT", out, quit);
    CHECK(out.str() == "LIST-DICT takes <dict-file>\n");

    out.str("");
    sh.handleLine("LIST-DICT DICT EXTRA", out, quit);
    CHECK(out.str() == "LIST-DICT takes <dict-file>\n");

    out.str("");
    sh.handleLine("LIST-DICT DICT", out, quit);
    CHECK(out.str().find("LIST-DICT requires dictionary file DICT") == 0);
}

TEST_CASE("shell LIST-DICT shows stable type and validity output") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("ALIAS", "S\nNAME\n"));
    fs.write("DICT", PickFS::Record("BADF", "F\n2\n\n"));
    fs.write("DICT", PickFS::Record("BADI", "I\n\n"));
    fs.write("DICT", PickFS::Record("GARBAGE", "???\n"));
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    fs.write("DICT", PickFS::Record("NET", "I\nA + B\n"));
    fs.write("DICT", PickFS::Record("THIRD", "F\n2\n3\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("LIST-DICT DICT", out, quit);
    CHECK(out.str() == "ALIAS S VALID\n"
                       "BADF F INVALID\n"
                       "BADI I INVALID\n"
                       "GARBAGE INVALID INVALID\n"
                       "NAME A VALID\n"
                       "NET I VALID\n"
                       "THIRD F VALID\n");
}

TEST_CASE("shell LIST with F-type field emits evaluated value") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("THIRD", "F\n2\n3\n"));
    const std::string body = std::string("X\n") + std::string("A") + static_cast<char>(0xFD) + "B" +
                             static_cast<char>(0xFD) + "C\n";
    fs.write("DATA", PickFS::Record("R1", body));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("LIST DATA THIRD", out, quit);
    CHECK(out.str().find("C") != std::string::npos);
    CHECK(out.str().find("R1") != std::string::npos);
}

TEST_CASE("shell LIST with F-type OCONV D field") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("PICKDAY", "F\n2\nD\n"));
    fs.write("DATA", PickFS::Record("R1", "X\n732\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("LIST DATA PICKDAY", out, quit);
    CHECK(out.str().find("01 Jan 1970") != std::string::npos);
}

TEST_CASE("shell DEFINE-FIELD arity missing dict and ENGLISH LIST") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("DEFINE-FIELD", out, quit);
    CHECK(out.str().find("DEFINE-FIELD takes") == 0);

    out.str("");
    sh.handleLine("DEFINE-FIELD DICT NM 1", out, quit);
    CHECK(out.str().find("CREATE-FILE first") != std::string::npos);

    sh.handleLine("CREATE-FILE DICT", out, quit);
    out.str("");
    sh.handleLine("DEFINE-FIELD DICT NM 0", out, quit);
    CHECK(out.str().find("positive integer") != std::string::npos);

    out.str("");
    sh.handleLine("DEFINE-FIELD DICT NM 1", out, quit);
    CHECK(out.str().empty());

    sh.handleLine("CREATE-FILE DATA", out, quit);
    sh.handleLine("WRITE DATA R1 ZEBRA", out, quit);
    sh.handleLine("WRITE DATA R2 ADAM", out, quit);
    out.str("");
    sh.handleLine("LIST DATA NM", out, quit);
    CHECK(out.str().find("ZEBRA") != std::string::npos);
    CHECK(out.str().find("ADAM") != std::string::npos);
}

TEST_CASE("shell DEFINE-FIELD DICT-DATA overrides global DICT for ENGLISH") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-FILE DATA", out, quit);
    sh.handleLine("CREATE-FILE DICT", out, quit);
    sh.handleLine("CREATE-FILE DICT-DATA", out, quit);
    sh.handleLine("DEFINE-FIELD DICT NM 2", out, quit);
    sh.handleLine("DEFINE-FIELD DICT-DATA NM 1", out, quit);
    PickFS::FileSystem fs(dir);
    fs.write("DATA", PickFS::Record("R1", "FIRST\nSECOND\n"));
    out.str("");
    sh.handleLine("LIST DATA NM", out, quit);
    CHECK(out.str().find("FIRST") != std::string::npos);
    CHECK(out.str().find("SECOND") == std::string::npos);
}

TEST_CASE("shell VOC verb alias routes ENGLISH LIST") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("VOC");
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("VOC", PickFS::Record("L", "V\nLIST\n"));
    fs.write("DICT", PickFS::Record("NAME", "A\n1\n"));
    fs.write("DATA", PickFS::Record("R1", "ALICE\n"));

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("L DATA NAME", out, quit);
    CHECK(out.str().find("R1 ALICE") != std::string::npos);
}

TEST_CASE("shell CREATE-VOC DELETE-VOC LIST-VOC arity and type checks") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("VOC");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-VOC", out, quit);
    CHECK(out.str() == "CREATE-VOC requires <item-id> <type> <target...>\n");

    out.str("");
    sh.handleLine("CREATE-VOC A1 Z BP", out, quit);
    CHECK(out.str() == "CREATE-VOC type must be one of A,D,F,Q,V,X\n");

    out.str("");
    sh.handleLine("DELETE-VOC", out, quit);
    CHECK(out.str() == "DELETE-VOC requires an item-id\n");

    out.str("");
    sh.handleLine("DELETE-VOC A1 EXTRA", out, quit);
    CHECK(out.str() == "DELETE-VOC requires an item-id\n");

    out.str("");
    sh.handleLine("LIST-VOC EXTRA", out, quit);
    CHECK(out.str() == "LIST-VOC takes no arguments\n");
}

TEST_CASE("shell CREATE-VOC writes canonical type and target attributes") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("VOC");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-VOC BP f BP", out, quit);
    CHECK(out.str().empty());
    const std::optional<PickFS::Record> rec = fs.read("VOC", "BP");
    REQUIRE(rec.has_value());
    CHECK(rec->value() == "F\nBP\n");
}

TEST_CASE("shell CREATE-VOC and DELETE-VOC invalidate resolver cache") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("VOC");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("HI", out, quit);
    CHECK(out.str() == "Unknown command: HI\n");

    out.str("");
    sh.handleLine("CREATE-VOC HI V WHO", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("HI", out, quit);
    CHECK(out.str() == "0 - -\n");

    out.str("");
    sh.handleLine("DELETE-VOC HI", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("HI", out, quit);
    CHECK(out.str() == "Unknown command: HI\n");
}

TEST_CASE("shell LIST-VOC shows stable item and type output with INVALID marker") {
    auto dir = uniqueTempDir();
    PickFS::FileSystem fs(dir);
    fs.createFile("VOC");
    fs.write("VOC", PickFS::Record("ZREC", "V\nWHO\n"));
    fs.write("VOC", PickFS::Record("AREC", "garbage\n"));
    fs.write("VOC", PickFS::Record("MREC", "001 F\n002 BP\n"));
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("LIST-VOC", out, quit);
    CHECK(out.str() == "AREC INVALID\nMREC F\nZREC V\n");
}

TEST_CASE("shell COUNT LIST SORT use active list when filename omitted") {
    auto dir = uniqueTempDir();
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    PickFS::FileSystem fs(dir);
    fs.createFile("DATA");
    fs.createFile("DICT");
    fs.write("DICT", PickFS::Record("NM", "A\n1\n"));
    fs.write("DATA", PickFS::Record("R2", "B\n"));
    fs.write("DATA", PickFS::Record("R1", "A\n"));

    sh.handleLine("SELECT DATA", out, quit);

    out.str("");
    sh.handleLine("COUNT", out, quit);
    CHECK(out.str() == "2\n");

    out.str("");
    sh.handleLine("LIST NM", out, quit);
    CHECK(out.str().find("R1 A") != std::string::npos);
    CHECK(out.str().find("R2 B") != std::string::npos);

    out.str("");
    sh.handleLine("SORT BY NM", out, quit);
    CHECK(out.str() == "R1\nR2\n");
}

TEST_CASE("shell filesystem command arity and missing record") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-FILE", out, quit);
    CHECK(out.str() == "CREATE-FILE requires a filename\n");

    out.str("");
    sh.handleLine("CREATE-FILE BP extra", out, quit);
    CHECK(out.str() == "CREATE-FILE requires a filename\n");

    out.str("");
    sh.handleLine("DELETE-FILE", out, quit);
    CHECK(out.str() == "DELETE-FILE requires a filename\n");

    out.str("");
    sh.handleLine("DELETE-FILE BP extra", out, quit);
    CHECK(out.str() == "DELETE-FILE requires a filename\n");

    out.str("");
    sh.handleLine("LIST-FILES extra", out, quit);
    CHECK(out.str() == "LIST-FILES takes no arguments\n");

    out.str("");
    sh.handleLine("READ BP", out, quit);
    CHECK(out.str().find("READ requires a file and record name") == 0);

    out.str("");
    sh.handleLine("WRITE BP HELLO", out, quit);
    CHECK(out.str().find("WRITE requires a file, record name, and value") == 0);
}

TEST_CASE("shell READ WRITE use MD DEFDATA default file and GET LIST-VARS SET") {
    const auto dir = uniqueTempDir();
    std::filesystem::create_directories(dir / "MD");
    {
        std::ofstream md(dir / "MD" / "DEFDATA.item");
        md << "DATA\n";
    }
    PickFS::FileSystem fsInit(dir);
    fsInit.createFile("DATA");

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(dir);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("GET @DEFDATA", out, quit);
    CHECK(out.str() == "DATA\n");

    out.str("");
    sh.handleLine("WRITE MYREC hello", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("READ MYREC", out, quit);
    CHECK(out.str() == "hello\n");

    out.str("");
    sh.handleLine("WRITE DATA MYREC hello world", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("READ MYREC", out, quit);
    CHECK(out.str() == "hello world\n");

    out.str("");
    sh.handleLine("LIST-VARS", out, quit);
    CHECK(out.str().find("@DEFDATA") != std::string::npos);

    out.str("");
    sh.handleLine("SET @DEFDATA X", out, quit);
    CHECK(out.str() == "Read-only system variable\n");
}

TEST_CASE("shell MD DEFDATA reloads when Pick root changes") {
    const auto dirWith = uniqueTempDir();
    std::filesystem::create_directories(dirWith / "MD");
    {
        std::ofstream md(dirWith / "MD" / "DEFDATA.item");
        md << "BP\n";
    }
    const auto dirWithout = uniqueTempDir();
    std::filesystem::create_directories(dirWithout);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.setFileSystemRoot(dirWith);
    sh.handleLine("GET @DEFDATA", out, quit);
    CHECK(out.str() == "BP\n");

    out.str("");
    sh.setFileSystemRoot(dirWithout);
    sh.handleLine("GET @DEFDATA", out, quit);
    CHECK(out.str() == "\n");
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
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramObjectRecord(fsDir, "tiny", "PUSH_INT 1\nHALT\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("ASM", out, quit);
    sh.handleLine("BREAKPOINT 5", out, quit);
    sh.handleLine("RUN tiny", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find("Removed invalid breakpoint(s): 5") != std::string::npos);
}

TEST_CASE("shell RUN TRACE ON includes HALT line") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramObjectRecord(fsDir, "t", "PUSH_INT 7\nHALT\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("ASM", out, quit);
    sh.handleLine("TRACE ON", out, quit);
    sh.handleLine("RUN t", out, quit);
    CHECK(out.str().find("0: PUSH_INT 7") != std::string::npos);
    CHECK(out.str().find("1: HALT") != std::string::npos);
}

TEST_CASE("shell DUMP-PROGRAM and DUMP-LABELS after RUN") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramObjectRecord(fsDir, "lbl", "start: PUSH_INT 3\nHALT\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
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
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramObjectRecord(fsDir, "bp", "PUSH_INT 10\nPUSH_INT 20\nADD\nHALT\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
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
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramObjectRecord(fsDir, "bp_meta", "PUSH_INT 10\nPUSH_INT 20\nADD\nHALT\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
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
    auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(fsDir);
    seedVocAndBp(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);

    std::istringstream editIn("REPLACE 1\n10 PRINT \"WORLD\"\n.\nFI\nQ\n");
    sh.setInputStream(&editIn);

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

    sh.setInputStream(nullptr);
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
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");
    sh.handleLine("SAVE HELLO", out, quit);
    CHECK(out.str().empty());
    PickFS::FileSystem fs(fsDir);
    CHECK(fs.read("BP", "HELLO").has_value());

    sh.handleLine("QUIT", out, quit);
    out.str("");
    sh.handleLine("BASIC HELLO", out, quit);
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 PRINT \"HELLO\"\n");
}

TEST_CASE("shell BASIC SAVE writes source only and not bytecode") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");
    sh.handleLine("SAVE HELLO", out, quit);
    CHECK(out.str().empty());
    PickFS::FileSystem fs(fsDir);
    CHECK(fs.read("BP", "HELLO").has_value());
    CHECK_FALSE(fs.read("BP", "HELLO_OBJ").has_value());
}

TEST_CASE("shell BASIC COMPILE writes bytecode and failed COMPILE preserves prior bytecode") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC HELLO", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");
    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Compiled successfully.\nInstructions: 4\nLabels: 0\n");
    PickFS::FileSystem fs(fsDir);
    const std::optional<PickFS::Record> beforeObj = fs.read("BP", "HELLO_OBJ");
    REQUIRE(beforeObj.has_value());

    sh.handleLine("20 ZAP A = 1", out, quit);
    out.str("");
    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Error on line 20: Unknown keyword 'ZAP'\nCompilation failed.\n");
    const std::optional<PickFS::Record> afterObj = fs.read("BP", "HELLO_OBJ");
    REQUIRE(afterObj.has_value());
    CHECK(afterObj->value() == beforeObj->value());
}

TEST_CASE("shell BASIC named entry missing file starts empty and creates on SAVE only") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC HELLO", out, quit);
    PickFS::FileSystem fs(fsDir);
    CHECK_FALSE(fs.read("BP", "HELLO").has_value());
    out.str("");

    sh.handleLine("LIST", out, quit);
    CHECK(out.str().empty());
    CHECK_FALSE(fs.read("BP", "HELLO").has_value());

    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");
    sh.handleLine("SAVE", out, quit);
    CHECK(out.str().empty());
    CHECK(fs.read("BP", "HELLO").has_value());
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
    sh.handleLine("30 ZAP A = 1", out, quit);
    out.str("");

    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Error on line 30: Unknown keyword 'ZAP'\nCompilation failed.\n");
}

TEST_CASE("shell BASIC RUN compile failure skips execution") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 ZAP A = 1", out, quit);
    sh.handleLine("20 PRINT \"SHOULD_NOT_RUN\"", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "Error on line 10: Unknown keyword 'ZAP'\nCompilation failed.\n");
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

TEST_CASE("shell BASIC RUN supports prompted INPUT syntax") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::istringstream in("123\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 INPUT \"Enter value: \", A", out, quit);
    sh.handleLine("20 PRINT A", out, quit);
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

TEST_CASE("shell BASIC COMPILE reports malformed prompted INPUT") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 INPUT , A", out, quit);
    out.str("");

    sh.handleLine("COMPILE", out, quit);
    CHECK(out.str() == "Error on line 10: INPUT prompt expression cannot be empty\nCompilation failed.\n");
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

TEST_CASE("shell BASIC EDIT delegates to LineRecordEditor unknown command then QUIT") {
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    std::istringstream in("XYZZY\nQ\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("BASIC TEST", out, quit);
    sh.handleLine("10 PRINT \"HELLO\"", out, quit);
    out.str("");
    sh.handleLine("EDIT", out, quit);
    CHECK(out.str().find("Unknown command") != std::string::npos);

    sh.setInputStream(nullptr);
    out.str("");
    sh.handleLine("LIST", out, quit);
    CHECK(out.str() == "10 PRINT \"HELLO\"\n");
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
    auto fsDir = uniqueTempDir();
    seedVocAndBp(fsDir);
    writeProgramObjectRecord(fsDir, "bp_end", "PUSH_INT 10\nPUSH_INT 20\nADD\nHALT\n");
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
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

TEST_CASE("shell BASIC RUN executes CHAIN to another BASIC program") {
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

    sh.handleLine("BASIC NEXT", out, quit);
    sh.handleLine("10 PRINT \"CHAINED\"", out, quit);
    sh.handleLine("20 END", out, quit);
    sh.handleLine("RUN (C", out, quit);
    out.str("");

    sh.handleLine("QUIT", out, quit);
    out.str("");

    sh.handleLine("BASIC MAIN", out, quit);
    sh.handleLine("10 CHAIN \"NEXT\"", out, quit);
    sh.handleLine("20 PRINT \"UNREACHED\"", out, quit);
    sh.handleLine("30 END", out, quit);
    out.str("");

    sh.handleLine("RUN", out, quit);
    CHECK(out.str() == "CHAINED\n");
}

TEST_CASE("shell EDIT file record INSERT SAVE QUIT then READ") {
    auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);

    std::istringstream in("I\nhello world\n.\nFI\nQ\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-FILE BP", out, quit);
    out.str("");

    sh.handleLine("EDIT BP MYREC", out, quit);
    CHECK(out.str().find("ED>") != std::string::npos);

    out.str("");
    sh.handleLine("READ BP MYREC", out, quit);
    CHECK(out.str() == "hello world\n");
}

TEST_CASE("shell EDIT uses aliases and case-insensitive LIST") {
    auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);

    std::istringstream in("I\nonly\n.\nL\nlist 1\nFI\nQ\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("CREATE-FILE BP", out, quit);
    out.str("");

    sh.handleLine("EDIT BP Z", out, quit);

    out.str("");
    sh.handleLine("READ BP Z", out, quit);
    CHECK(out.str() == "only\n");
}

TEST_CASE("shell EDIT REPLACE multiline and DELETE") {
    auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);

    PickFS::FileSystem fsInit(fsDir);
    fsInit.createFile("BP");
    writeRecord(fsDir, "BP", "RREC", "a\nb\nc");

    std::istringstream in("REPLACE 2\nx\ny\n.\nDELETE 3\nFI\nQ\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("EDIT BP RREC", out, quit);

    out.str("");
    sh.handleLine("READ BP RREC", out, quit);
    CHECK(out.str() == "a\nx\nc\n");
}

TEST_CASE("shell EDIT program name shorthand resolves VOC source record") {
    auto fsDir = uniqueTempDir();
    std::filesystem::create_directories(fsDir);

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(fsDir);
    seedVocAndBp(fsDir);
    writeProgramSourceRecord(fsDir, "HELLO", "10 PRINT 1\n");

    std::istringstream in("I\n20 PRINT 2\n.\nFI\nQ\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("EDIT HELLO", out, quit);

    out.str("");
    sh.handleLine("READ BP HELLO", out, quit);
    CHECK(out.str() == "10 PRINT 1\n20 PRINT 2\n");
}

TEST_CASE("shell EDIT arity errors") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("EDIT", out, quit);
    CHECK(out.str().find("EDIT requires") != std::string::npos);

    out.str("");
    sh.handleLine("EDIT A B C", out, quit);
    CHECK(out.str().find("EDIT takes") != std::string::npos);
}

TEST_CASE("shell HELP EDIT shows edit usage line") {
    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("HELP EDIT", out, quit);
    CHECK(out.str().find("EDIT <file> <record>") != std::string::npos);
}

TEST_CASE("shell WHO after attachUserSession shows session") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    std::filesystem::create_directories(gem / "accounts" / "TST" / "VOC");
    {
        std::ofstream vocEntry(gem / "accounts" / "TST" / "VOC" / "BP.item");
        vocEntry << "F\nBP\n";
    }
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"}]})";
    }

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setGeminiCatalogRoot(gem);
    std::ostringstream err;
    const auto session = PickCore::LoginService::authenticateAccount(gem, "TST", "", err);
    REQUIRE(session.has_value());
    CHECK(err.str().empty());
    sh.attachUserSession(*session);

    std::ostringstream out;
    bool quit = false;
    sh.handleLine("WHO", out, quit);
    CHECK(out.str() == "0 TST TST\n");
}

TEST_CASE("shell LOGTO applies catalogue session like fresh logon") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    for (const char *acc: {"TST", "OTH"}) {
        std::filesystem::create_directories(gem / "accounts" / acc / "VOC");
        std::ofstream vocEntry(gem / "accounts" / acc / "VOC" / "BP.item");
        vocEntry << "F\nBP\n";
    }
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"},{"name":"OTH","root":"accounts/OTH"}]})";
    }

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setGeminiCatalogRoot(gem);
    std::ostringstream err;
    const auto session = PickCore::LoginService::authenticateAccount(gem, "TST", "", err);
    REQUIRE(session.has_value());
    CHECK(err.str().empty());
    sh.attachUserSession(*session);

    std::ostringstream out;
    bool quit = false;
    sh.handleLine("WHO", out, quit);
    CHECK(out.str() == "0 TST TST\n");

    out.str("");
    sh.handleLine("LOGTO OTH", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("WHO", out, quit);
    CHECK(out.str() == "0 OTH OTH\n");

    out.str("");
    sh.handleLine("GET @LOGNAME", out, quit);
    CHECK(out.str() == "OTH\n");

    out.str("");
    sh.handleLine("GET @USERNO", out, quit);
    CHECK(out.str() == "0\n");
}

TEST_CASE("shell LOGTO reads password line when target account requires it") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    for (const char *acc: {"TST", "PWD"}) {
        std::filesystem::create_directories(gem / "accounts" / acc / "VOC");
        std::ofstream vocEntry(gem / "accounts" / acc / "VOC" / "BP.item");
        vocEntry << "F\nBP\n";
    }
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"},{"name":"PWD","root":"accounts/PWD","passwordHash":"secret"}]})";
    }

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setGeminiCatalogRoot(gem);
    std::ostringstream err;
    const auto session = PickCore::LoginService::authenticateAccount(gem, "TST", "", err);
    REQUIRE(session.has_value());
    sh.attachUserSession(*session);

    std::istringstream in("secret\n");
    sh.setInputStream(&in);
    std::ostringstream out;
    bool quit = false;
    sh.handleLine("LOGTO PWD", out, quit);
    CHECK(out.str().empty());

    out.str("");
    sh.handleLine("WHO", out, quit);
    CHECK(out.str() == "0 PWD PWD\n");
}

TEST_CASE("shell session @ system variables Tcl GET SET LIST-VARS PROC and BASIC RUN") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    std::filesystem::create_directories(gem / "accounts" / "TST" / "VOC");
    {
        std::ofstream vocEntry(gem / "accounts" / "TST" / "VOC" / "BP.item");
        vocEntry << "F\nBP\n";
    }
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"}]})";
    }

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setGeminiCatalogRoot(gem);
    std::ostringstream err;
    const auto session = PickCore::LoginService::authenticateAccount(gem, "TST", "", err);
    REQUIRE(session.has_value());
    CHECK(err.str().empty());
    sh.attachUserSession(*session);

    PickFS::FileSystem fsPick(session->pickRoot);
    fsPick.createFile("BP");
    fsPick.write("BP", PickFS::Record("SVAR", "10 PRINT @ACCOUNT\n20 END\n"));
    fsPick.createFile("PROC");
    writeProcScriptRecord(session->pickRoot, "SESS", "DISPLAY @LOGNAME\nTCL ECHO @ACCOUNT\nEND\n");

    std::ostringstream out;
    bool quit = false;

    sh.handleLine("ECHO @account @LOGNAME", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "TST TST\n");

    out.str("");
    sh.handleLine("GET @USERNO", out, quit);
    CHECK(out.str() == "0\n");

    out.str("");
    sh.handleLine("SET @ACCOUNT x", out, quit);
    CHECK(out.str() == "Read-only system variable\n");

    out.str("");
    sh.handleLine("UNSET @USERNO", out, quit);
    CHECK(out.str() == "Read-only system variable\n");

    out.str("");
    sh.handleLine("SET zed 1", out, quit);
    out.str("");
    sh.handleLine("LIST-VARS", out, quit);
    CHECK(out.str() == "Variables:\n  @ACCOUNT\n  @LOGNAME\n  @USERNO\n  ZED\n");

    out.str("");
    sh.handleLine("PROC SESS", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str() == "TST\nTST\n");

    out.str("");
    sh.handleLine("RUN SVAR", out, quit);
    CHECK(out.str() == "TST\n");
}
