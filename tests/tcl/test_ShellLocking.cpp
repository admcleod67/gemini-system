#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "FileSystem.h"
#include "GeminiSession.h"
#include "Shell.h"
#include "ShellTestFsUtil.h"
#include "UserSession.h"

using PickTests::uniqueTempDir;

namespace {
    struct LoggedInShellPair {
        std::filesystem::path pickRoot;
        std::unique_ptr<PickShell::GeminiSession> sessionA;
        std::unique_ptr<PickShell::GeminiSession> sessionB;
        PickShell::Shell *shellA{nullptr};
        PickShell::Shell *shellB{nullptr};

        LoggedInShellPair()
            : sessionA(std::make_unique<PickShell::GeminiSession>()),
              sessionB(std::make_unique<PickShell::GeminiSession>()),
              shellA(&sessionA->shell()),
              shellB(&sessionB->shell()) {
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

    void seedVocRecord(const std::filesystem::path &pickRoot, const std::string &recordName, const std::string &value) {
        PickFS::FileSystem fs(pickRoot);
        fs.createFile("VOC");
        fs.write("VOC", PickFS::Record(recordName, value));
    }

    [[nodiscard]] LoggedInShellPair makeLoggedInShellPair() {
        const auto root = uniqueTempDir();
        const auto gem = root / "gemini";
        const auto pickRoot = gem / "accounts" / "TST";
        std::filesystem::create_directories(pickRoot / "MD");
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

TEST_CASE("shell READU blocks peer READ when logged in") {
    auto pair = makeLoggedInShellPair();
    std::ostringstream outA;
    std::ostringstream outB;

    handle(*pair.shellA, "READU DATA R1", outA);
    CHECK(outA.str() == "seed\n");

    handle(*pair.shellB, "READ DATA R1", outB);
    CHECK(outB.str().find("RECORD LOCKED") != std::string::npos);
}

TEST_CASE("shell same session READU then READ succeeds") {
    auto pair = makeLoggedInShellPair();
    std::ostringstream out;

    handle(*pair.shellA, "READU DATA R1", out);
    CHECK(out.str() == "seed\n");

    out.str("");
    handle(*pair.shellA, "READ DATA R1", out);
    CHECK(out.str() == "seed\n");
}

TEST_CASE("shell WRITEU blocks peer WRITE when logged in") {
    auto pair = makeLoggedInShellPair();
    std::ostringstream outA;
    std::ostringstream outB;

    handle(*pair.shellA, "WRITEU DATA R1 locked", outA);
    CHECK(outA.str().empty());

    handle(*pair.shellB, "WRITE DATA R1 other", outB);
    CHECK(outB.str().find("RECORD LOCKED") != std::string::npos);
}

TEST_CASE("shell RELEASE unblocks peer READU") {
    auto pair = makeLoggedInShellPair();
    std::ostringstream outA;
    std::ostringstream outB;

    handle(*pair.shellA, "READU DATA R1", outA);
    handle(*pair.shellA, "RELEASE DATA R1", outA);
    handle(*pair.shellB, "READU DATA R1", outB);
    CHECK(outB.str() == "seed\n");
}

TEST_CASE("shell READU conflicts with peer READU") {
    auto pair = makeLoggedInShellPair();
    std::ostringstream outA;
    std::ostringstream outB;

    handle(*pair.shellA, "READU DATA R1", outA);
    handle(*pair.shellB, "READU DATA R1", outB);
    CHECK(outB.str().find("RECORD LOCKED") != std::string::npos);
}

TEST_CASE("shell READU DEFDATA shorthand blocks peer READ") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pickRoot = gem / "accounts" / "TST";
    std::filesystem::create_directories(pickRoot / "MD");
    {
        std::ofstream md(pickRoot / "MD" / "DEFDATA.item");
        md << "DATA\n";
    }
    seedDataRecord(pickRoot, "R1", "seed");

    PickShell::GeminiSession gsA;
    PickShell::GeminiSession gsB;
    PickShell::Shell &shellA = gsA.shell();
    PickShell::Shell &shellB = gsB.shell();
    shellA.attachUserSession(makeUserSession(gem, pickRoot, 1, "USERA"));
    shellB.attachUserSession(makeUserSession(gem, pickRoot, 2, "USERB"));

    std::ostringstream outA;
    std::ostringstream outB;
    handle(shellA, "READU R1", outA);
    handle(shellB, "READ R1", outB);
    CHECK(outB.str().find("RECORD LOCKED") != std::string::npos);
}

TEST_CASE("shell READU without login does not lock across shells") {
    const auto pickRoot = uniqueTempDir();
    seedDataRecord(pickRoot, "R1", "seed");

    PickShell::GeminiSession gsA;
    PickShell::GeminiSession gsB;
    PickShell::Shell &shellA = gsA.shell();
    PickShell::Shell &shellB = gsB.shell();
    shellA.setFileSystemRoot(pickRoot);
    shellB.setFileSystemRoot(pickRoot);

    std::ostringstream outA;
    std::ostringstream outB;
    handle(shellA, "READU DATA R1", outA);
    handle(shellB, "READ DATA R1", outB);
    CHECK(outA.str() == "seed\n");
    CHECK(outB.str() == "seed\n");
}

TEST_CASE("shell DELETE-VOC blocked when peer holds READU on VOC entry") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pickRoot = gem / "accounts" / "TST";
    std::filesystem::create_directories(pickRoot);
    seedVocRecord(pickRoot, "HI", "F\nBP\n");

    PickShell::GeminiSession gsA;
    PickShell::GeminiSession gsB;
    PickShell::Shell &shellA = gsA.shell();
    PickShell::Shell &shellB = gsB.shell();
    shellA.attachUserSession(makeUserSession(gem, pickRoot, 1, "USERA"));
    shellB.attachUserSession(makeUserSession(gem, pickRoot, 2, "USERB"));

    std::ostringstream outA;
    std::ostringstream outB;
    handle(shellA, "LIST-VOC", outA);
    handle(shellB, "LIST-VOC", outB);
    outA.str("");
    outB.str("");
    handle(shellA, "READU VOC HI", outA);
    handle(shellB, "DELETE-VOC HI", outB);
    CHECK(outB.str().find("RECORD LOCKED") != std::string::npos);
}

TEST_CASE("shell READU WRITEU RELEASE arity diagnostics") {
    PickShell::GeminiSession gs;
    PickVM::Runtime &rt = gs.runtime();
    PickShell::Shell &sh = gs.shell();
    std::ostringstream out;
    bool quit = false;

    sh.handleLine("READU", out, quit);
    CHECK(out.str().find("READU requires a file and record name") == 0);

    out.str("");
    sh.handleLine("WRITEU DATA R1", out, quit);
    CHECK(out.str().find("WRITEU requires a file, record name, and value") == 0);

    out.str("");
    sh.handleLine("RELEASE", out, quit);
    CHECK(out.str().find("RELEASE requires a file and record name") == 0);
}
