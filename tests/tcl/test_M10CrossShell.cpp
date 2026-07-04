#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "BasicLanguageRegistration.h"
#include "FileSystem.h"
#include "LanguageRegistry.h"
#include "GeminiSession.h"
#include "Shell.h"
#include "ShellTestFsUtil.h"
#include "UserSession.h"

using PickTests::uniqueTempDir;
using PickTests::writeProcScriptRecord;
using PickTests::writeRecord;

namespace {
    struct LoggedInShellPair {
        std::filesystem::path pickRoot;
        PickCore::Languages::LanguageRegistry languageRegistry;
        std::unique_ptr<PickShell::GeminiSession> sessionA;
        std::unique_ptr<PickShell::GeminiSession> sessionB;
        PickShell::Shell *shellA{nullptr};
        PickShell::Shell *shellB{nullptr};

        LoggedInShellPair()
            : sessionA(std::make_unique<PickShell::GeminiSession>()),
              sessionB(std::make_unique<PickShell::GeminiSession>()),
              shellA(&sessionA->shell()),
              shellB(&sessionB->shell()) {
            PickCore::Languages::Basic::registerBasicLanguage(languageRegistry);
            sessionA->runtime().setLanguageRegistry(&languageRegistry);
            sessionB->runtime().setLanguageRegistry(&languageRegistry);
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

    void seedVocProcAndBp(const std::filesystem::path &pickRoot) {
        PickFS::FileSystem fs(pickRoot);
        fs.createFile("VOC");
        fs.createFile("PROC");
        fs.createFile("BP");
        fs.write("VOC", PickFS::Record("BP", "001 F\n002 BP\n003 /gemini/fs/DM/BP\n"));
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
        seedVocProcAndBp(pickRoot);
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

TEST_CASE("cross-shell Tcl READU blocks PROC native READU with ?LOCKED?") {
    auto pair = makeLoggedInShellPair();
    writeProcScriptRecord(pair.pickRoot, "TRYR",
                          "READU REC DATA R1\n"
                          "DISPLAY PROCERR\n"
                          "END\n");
    writeProcScriptRecord(pair.pickRoot, "REL",
                          "RELEASE DATA R1\n"
                          "END\n");

    std::ostringstream outA;
    std::ostringstream outB;
    handle(*pair.shellA, "READU DATA R1", outA);
    handle(*pair.shellB, "PROC TRYR", outB);
    CHECK(outB.str() == "?LOCKED?\n");

    outB.str("");
    handle(*pair.shellA, "RELEASE DATA R1", outB);
    handle(*pair.shellB, "PROC TRYR", outB);
    CHECK(outB.str() == "\n");
}

TEST_CASE("cross-shell PROC READU blocks Tcl READ with RECORD LOCKED") {
    auto pair = makeLoggedInShellPair();
    writeProcScriptRecord(pair.pickRoot, "LOCKR",
                          "READU REC DATA R1\n"
                          "END\n");

    std::ostringstream outA;
    std::ostringstream outB;
    handle(*pair.shellA, "PROC LOCKR", outA);
    handle(*pair.shellB, "READ DATA R1", outB);
    CHECK(outB.str().find("RECORD LOCKED") != std::string::npos);
}

TEST_CASE("cross-shell Tcl READU blocks BASIC READU STATUS reporting") {
    auto pair = makeLoggedInShellPair();
    writeRecord(pair.pickRoot, "BP", "LOCKTEST",
                "10 OPEN \"DATA\" TO F\n"
                "20 ON ERROR GOTO 100\n"
                "30 READU REC FROM F, \"R1\"\n"
                "40 PRINT STATUS()\n"
                "50 STOP\n"
                "100 PRINT STATUS()\n"
                "110 STOP\n");

    std::ostringstream outA;
    std::ostringstream outB;
    handle(*pair.shellA, "READU DATA R1", outA);
    handle(*pair.shellB, "RUN LOCKTEST", outB);
    CHECK(outB.str() == "1\n");
}
