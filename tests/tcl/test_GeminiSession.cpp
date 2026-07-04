#include <doctest/doctest.h>

#include <pick_system/version.hpp>

#include "GeminiSession.h"
#include "Runtime.h"
#include "UserSession.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using PickVM::Instruction;
using PickVM::OpCode;
using PickVM::Runtime;

TEST_CASE("GeminiSession constructs and exposes stable runtime") {
    PickShell::GeminiSession session;
    PickVM::Runtime *const runtimeAddr = &session.runtime();
    CHECK(runtimeAddr == &session.runtime());
}

TEST_CASE("GeminiSession shell VERSION") {
    PickShell::GeminiSession session;
    std::ostringstream out;
    bool quit = false;
    session.shell().handleLine("VERSION", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find(pick_system::version_string) != std::string::npos);
}

TEST_CASE("GeminiSession setOutputStream wires runtime PRINT") {
    std::ostringstream out;
    PickShell::GeminiSession session;
    session.setOutputStream(&out);

    const std::vector<Instruction> prog = {
        {OpCode::PushInt, 42},
        {OpCode::PrintInt, PickVM::Value{}},
        {OpCode::PushStr, std::string{"ok"}},
        {OpCode::PrintStr, PickVM::Value{}},
        {OpCode::Halt, PickVM::Value{}},
    };
    session.runtime().loadProgram(prog);
    session.runtime().run();
    CHECK(out.str() == "42ok");
}

TEST_CASE("GeminiSession setInputStream wires runtime INPUT") {
    std::istringstream in("  hello world  \n");
    PickShell::GeminiSession session;
    session.setInputStream(&in);

    const std::vector<Instruction> prog = {
        {OpCode::InputStr, PickVM::Value{}},
        {OpCode::Halt, PickVM::Value{}},
    };
    session.runtime().loadProgram(prog);
    session.runtime().run();
    REQUIRE(session.runtime().stack().size() == 1);
    REQUIRE(std::holds_alternative<std::string>(session.runtime().stack()[0]));
    CHECK(std::get<std::string>(session.runtime().stack()[0]) == "hello world");
}

TEST_CASE("GeminiSession diagnostic defaults to cerr") {
    PickShell::GeminiSession session;
    CHECK(&session.diagnostic() == &std::cerr);
    CHECK(session.diagnosticStream() == nullptr);
}

TEST_CASE("GeminiSession setDiagnosticStream overrides diagnostic accessor") {
    std::ostringstream err;
    PickShell::GeminiSession session;
    session.setDiagnosticStream(&err);
    CHECK(&session.diagnostic() == &err);
    CHECK(session.diagnosticStream() == &err);
}

TEST_CASE("GeminiSession runTclRepl uses session I/O") {
    std::istringstream in("VERSION\nQUIT\n");
    std::ostringstream out;
    PickShell::GeminiSession session;
    session.setInputStream(&in);
    session.setOutputStream(&out);
    const PickShell::ShellRunResult result = session.shell().runTclRepl();
    CHECK(result == PickShell::ShellRunResult::ExitProcess);
    CHECK(out.str().find(pick_system::version_string) != std::string::npos);
    CHECK(out.str().find("Exiting shell") != std::string::npos);
}

TEST_CASE("GeminiSession attach applies pick root and session identity") {
    const auto root = std::filesystem::temp_directory_path() / "pick-gemini-attach-test";
    std::filesystem::create_directories(root / "MD");

    PickCore::UserSession user;
    user.catalogRoot = root / "gemini";
    user.pickRoot = root;
    user.accountName = "TST";
    user.username = "USERA";
    user.whoPort = 7;
    user.userNo = "7";

    PickShell::GeminiSession session;
    session.attach(user);

    CHECK(session.loggedIn());
    CHECK(session.whoPort() == 7);
    CHECK(session.sessionUsername() == "USERA");
    CHECK(session.sessionAccount() == "TST");
    CHECK(session.fileSystemRoot() == root);
    CHECK(session.sessionLockId() == PickShell::GeminiSession::makeSessionLockId(7, "TST", "USERA"));
}

TEST_CASE("GeminiSession attachUserSession via shell delegates to attach") {
    const auto root = std::filesystem::temp_directory_path() / "pick-gemini-shell-attach-test";
    std::filesystem::create_directories(root / "MD");

    PickCore::UserSession user;
    user.catalogRoot = root / "gemini";
    user.pickRoot = root;
    user.accountName = "ACCT";
    user.username = "BOB";
    user.whoPort = 3;
    user.userNo = "3";

    PickShell::GeminiSession session;
    session.shell().attachUserSession(user);

    CHECK(session.loggedIn());
    CHECK(session.sessionUsername() == "BOB");
    CHECK(session.sessionLockId() == PickShell::GeminiSession::makeSessionLockId(3, "ACCT", "BOB"));
}

TEST_CASE("GeminiSession create returns wired session") {
    const auto session = PickShell::GeminiSession::create();
    REQUIRE(session != nullptr);
    PickVM::Runtime *const runtimeAddr = &session->runtime();
    CHECK(runtimeAddr == &session->runtime());

    std::ostringstream out;
    bool quit = false;
    session->shell().handleLine("VERSION", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find(pick_system::version_string) != std::string::npos);
}

TEST_CASE("GeminiSession attach then detach clears login identity") {
    const auto root = std::filesystem::temp_directory_path() / "pick-gemini-detach-test";
    std::filesystem::create_directories(root / "MD");

    PickCore::UserSession user;
    user.catalogRoot = root / "gemini";
    user.pickRoot = root;
    user.accountName = "TST";
    user.username = "USERA";
    user.whoPort = 2;
    user.userNo = "2";

    PickShell::GeminiSession session;
    session.attach(user);
    REQUIRE(session.loggedIn());

    session.detach();
    CHECK_FALSE(session.loggedIn());
    CHECK(session.sessionLockId().empty());
    CHECK(session.whoPort() == 0);
    CHECK(session.sessionUsername().empty());
    CHECK(session.sessionAccount().empty());
}

TEST_CASE("GeminiSession reset clears interpreter state but preserves login") {
    PickShell::GeminiSession session;
    REQUIRE(session.env().set("X", "1"));

    const std::vector<Instruction> prog = {
        {OpCode::PushInt, 1},
        {OpCode::Halt, PickVM::Value{}},
    };
    session.runtime().loadProgram(prog);
    REQUIRE(session.runtime().isLoaded());

    session.setLoggedIn(true);
    session.setSessionIdentity(1, "U", "A");

    PickVM::Runtime *const runtimeAddr = &session.runtime();
    session.reset();

    CHECK(runtimeAddr == &session.runtime());
    CHECK(session.loggedIn());
    CHECK(session.sessionUsername() == "U");
    CHECK_FALSE(session.env().get("X").has_value());
    CHECK_FALSE(session.runtime().isLoaded());
}

TEST_CASE("GeminiSession detach then reset is LOGOFF-equivalent clean slate") {
    const auto root = std::filesystem::temp_directory_path() / "pick-gemini-logoff-test";
    std::filesystem::create_directories(root / "MD");

    PickCore::UserSession user;
    user.catalogRoot = root / "gemini";
    user.pickRoot = root;
    user.accountName = "TST";
    user.username = "USERA";
    user.whoPort = 4;
    user.userNo = "4";

    PickShell::GeminiSession session;
    session.attach(user);
    REQUIRE(session.env().set("TOKEN", "abc"));

    session.detach();
    session.reset();

    CHECK_FALSE(session.loggedIn());
    CHECK(session.sessionLockId().empty());
    CHECK_FALSE(session.env().get("TOKEN").has_value());
}

TEST_CASE("GeminiSession attach detach attach rebinds identity") {
    const auto root = std::filesystem::temp_directory_path() / "pick-gemini-reattach-test";
    std::filesystem::create_directories(root / "MD");

    PickCore::UserSession first;
    first.catalogRoot = root / "gemini";
    first.pickRoot = root;
    first.accountName = "TST";
    first.username = "USERA";
    first.whoPort = 1;
    first.userNo = "1";

    PickCore::UserSession second = first;
    second.username = "USERB";
    second.whoPort = 2;
    second.userNo = "2";

    PickShell::GeminiSession session;
    session.attach(first);
    CHECK(session.sessionLockId() == PickShell::GeminiSession::makeSessionLockId(1, "TST", "USERA"));

    session.detach();
    session.attach(second);

    CHECK(session.loggedIn());
    CHECK(session.sessionUsername() == "USERB");
    CHECK(session.sessionLockId() == PickShell::GeminiSession::makeSessionLockId(2, "TST", "USERB"));
}

TEST_CASE("GeminiSession destroy releases login and clears interpreter state") {
    const auto root = std::filesystem::temp_directory_path() / "pick-gemini-destroy-test";
    std::filesystem::create_directories(root / "MD");

    PickCore::UserSession user;
    user.catalogRoot = root / "gemini";
    user.pickRoot = root;
    user.accountName = "TST";
    user.username = "USERA";
    user.whoPort = 5;
    user.userNo = "5";

    auto session = PickShell::GeminiSession::create();
    session->attach(user);
    REQUIRE(session->env().set("Y", "2"));

    session->destroy();

    CHECK_FALSE(session->loggedIn());
    CHECK(session->sessionLockId().empty());
    CHECK_FALSE(session->env().get("Y").has_value());
    CHECK(session->inputStream() == nullptr);
    CHECK(session->outputStream() == nullptr);
    CHECK(session->diagnosticStream() == nullptr);
}
