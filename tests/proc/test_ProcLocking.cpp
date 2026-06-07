#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "FileSystem.h"
#include "Shell.h"
#include "ShellTestFsUtil.h"
#include "UserSession.h"

using PickTests::seedVocAndProc;
using PickTests::uniqueTempDir;
using PickTests::writeProcScriptRecord;

namespace {
    struct LoggedInShellPair {
        std::filesystem::path pickRoot;
        std::unique_ptr<PickVM::Runtime> rtA;
        std::unique_ptr<PickVM::Runtime> rtB;
        std::unique_ptr<PickShell::Shell> shellA;
        std::unique_ptr<PickShell::Shell> shellB;

        LoggedInShellPair()
            : rtA(std::make_unique<PickVM::Runtime>()),
              rtB(std::make_unique<PickVM::Runtime>()),
              shellA(std::make_unique<PickShell::Shell>(*rtA)),
              shellB(std::make_unique<PickShell::Shell>(*rtB)) {
        }
    };

    [[nodiscard]] PickCore::UserSession makeUserSession(const std::filesystem::path &gem,
                                                          const std::filesystem::path &pickRoot,
                                                          const int port,
                                                          const std::string &username) {
        PickCore::UserSession session;
        session.catalogRoot = gem;
        session.pickRoot = pickRoot;
        session.accountName = "TST";
        session.username = username;
        session.whoPort = port;
        session.userNo = std::to_string(port);
        return session;
    }

    void seedDataRecord(const std::filesystem::path &pickRoot, const std::string &recordName, const std::string &value) {
        PickFS::FileSystem fs(pickRoot);
        fs.createFile("DATA");
        fs.write("DATA", PickFS::Record(recordName, value));
    }

    [[nodiscard]] LoggedInShellPair makeLoggedInShellPair() {
        const auto root = uniqueTempDir();
        const auto gem = root / "gemini";
        const auto pickRoot = gem / "accounts" / "TST";
        std::filesystem::create_directories(pickRoot / "MD");
        seedVocAndProc(pickRoot);
        seedDataRecord(pickRoot, "R1", "seed");

        LoggedInShellPair pair;
        pair.pickRoot = pickRoot;
        pair.shellA->attachUserSession(makeUserSession(gem, pickRoot, 1, "USERA"));
        pair.shellB->attachUserSession(makeUserSession(gem, pickRoot, 2, "USERB"));
        return pair;
    }

    void handle(PickShell::Shell &shell, const std::string &line, std::ostringstream &out) {
        bool quit = false;
        shell.handleLine(line, out, quit);
    }
} // namespace

TEST_CASE("shell PROC READU assigns record and clears PROCERR on success") {
    auto pair = makeLoggedInShellPair();
    writeProcScriptRecord(pair.pickRoot, "GETR",
                          "READU REC DATA R1\n"
                          "DISPLAY REC\n"
                          "DISPLAY PROCERR\n"
                          "END\n");

    std::ostringstream out;
    handle(*pair.shellA, "PROC GETR", out);
    CHECK(out.str() == "seed\n\n");
}

TEST_CASE("shell PROC READU lock conflict sets PROCERR to ?LOCKED?") {
    auto pair = makeLoggedInShellPair();
    writeProcScriptRecord(pair.pickRoot, "TRYR",
                          "READU REC DATA R1\n"
                          "DISPLAY PROCERR\n"
                          "END\n");

    std::ostringstream outA;
    std::ostringstream outB;
    handle(*pair.shellA, "PROC TRYR", outA);
    handle(*pair.shellB, "PROC TRYR", outB);
    CHECK(outA.str() == "\n");
    CHECK(outB.str() == "?LOCKED?\n");
    CHECK(outB.str().find("RECORD LOCKED") == std::string::npos);
}

TEST_CASE("shell PROC RELEASE unblocks peer READU") {
    auto pair = makeLoggedInShellPair();
    writeProcScriptRecord(pair.pickRoot, "LOCKR",
                          "READU REC DATA R1\n"
                          "DISPLAY PROCERR\n"
                          "END\n");
    writeProcScriptRecord(pair.pickRoot, "REL",
                          "RELEASE DATA R1\n"
                          "END\n");

    std::ostringstream outA;
    std::ostringstream outB;
    handle(*pair.shellA, "PROC LOCKR", outA);
    handle(*pair.shellB, "PROC LOCKR", outB);
    CHECK(outB.str() == "?LOCKED?\n");

    outB.str("");
    handle(*pair.shellA, "PROC REL", outB);
    handle(*pair.shellB, "PROC LOCKR", outB);
    CHECK(outB.str() == "\n");
}

TEST_CASE("shell PROC WRITEU lock conflict sets PROCERR") {
    auto pair = makeLoggedInShellPair();
    writeProcScriptRecord(pair.pickRoot, "LOCKW",
                          "READU REC DATA R1\n"
                          "END\n");
    writeProcScriptRecord(pair.pickRoot, "TRYW",
                          "WRITEU DATA R1 new-value\n"
                          "DISPLAY PROCERR\n"
                          "END\n");

    std::ostringstream outA;
    std::ostringstream outB;
    handle(*pair.shellA, "PROC LOCKW", outA);
    handle(*pair.shellB, "PROC TRYW", outB);
    CHECK(outB.str() == "?LOCKED?\n");
}

TEST_CASE("shell PROC READU DEFDATA shorthand with lock conflict") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pickRoot = gem / "accounts" / "TST";
    std::filesystem::create_directories(pickRoot / "MD");
    {
        std::ofstream md(pickRoot / "MD" / "DEFDATA.item");
        md << "DATA\n";
    }
    seedVocAndProc(pickRoot);
    seedDataRecord(pickRoot, "R1", "seed");
    writeProcScriptRecord(pickRoot, "DEFR",
                          "READU REC R1\n"
                          "DISPLAY PROCERR\n"
                          "END\n");

    PickVM::Runtime rtA;
    PickVM::Runtime rtB;
    PickShell::Shell shellA(rtA);
    PickShell::Shell shellB(rtB);
    shellA.attachUserSession(makeUserSession(gem, pickRoot, 1, "USERA"));
    shellB.attachUserSession(makeUserSession(gem, pickRoot, 2, "USERB"));

    std::ostringstream outA;
    std::ostringstream outB;
    handle(shellA, "PROC DEFR", outA);
    handle(shellB, "PROC DEFR", outB);
    CHECK(outB.str() == "?LOCKED?\n");
}

TEST_CASE("shell PROC READU without login does not lock across shells") {
    const auto pickRoot = uniqueTempDir();
    seedVocAndProc(pickRoot);
    seedDataRecord(pickRoot, "R1", "seed");
    writeProcScriptRecord(pickRoot, "GETR",
                          "READU REC DATA R1\n"
                          "DISPLAY REC\n"
                          "END\n");

    PickVM::Runtime rtA;
    PickVM::Runtime rtB;
    PickShell::Shell shellA(rtA);
    PickShell::Shell shellB(rtB);
    shellA.setFileSystemRoot(pickRoot);
    shellB.setFileSystemRoot(pickRoot);

    std::ostringstream outA;
    std::ostringstream outB;
    handle(shellA, "PROC GETR", outA);
    handle(shellB, "PROC GETR", outB);
    CHECK(outA.str() == "seed\n");
    CHECK(outB.str() == "seed\n");
}

TEST_CASE("shell PROC IF PROCERR = ?LOCKED? branches on lock conflict") {
    auto pair = makeLoggedInShellPair();
    writeProcScriptRecord(pair.pickRoot, "LOCKR",
                          "READU REC DATA R1\n"
                          "END\n");
    writeProcScriptRecord(pair.pickRoot, "BRANCH",
                          "READU REC DATA R1\n"
                          "IF PROCERR = ?LOCKED? THEN DISPLAY LOCKED\n"
                          "IF PROCERR = ?LOCKED? THEN GO DONE\n"
                          "DISPLAY OK\n"
                          "DONE:\n"
                          "END\n");

    std::ostringstream outA;
    std::ostringstream outB;
    handle(*pair.shellA, "PROC LOCKR", outA);
    handle(*pair.shellB, "PROC BRANCH", outB);
    CHECK(outB.str() == "LOCKED\n");
    CHECK(outB.str().find("OK") == std::string::npos);
}

TEST_CASE("shell PROC READU rejects malformed arity") {
    const auto pickRoot = uniqueTempDir();
    seedVocAndProc(pickRoot);
    writeProcScriptRecord(pickRoot, "BAD",
                          "READU REC\n"
                          "END\n");

    PickVM::Runtime rt;
    PickShell::Shell sh(rt);
    sh.setFileSystemRoot(pickRoot);
    std::ostringstream out;
    handle(sh, "PROC BAD", out);
    CHECK(out.str().find("READU requires") != std::string::npos);
}
