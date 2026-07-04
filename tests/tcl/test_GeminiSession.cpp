#include <doctest/doctest.h>

#include <pick_system/version.hpp>

#include "GeminiSession.h"
#include "Runtime.h"
#include "UserSession.h"

#include <filesystem>
#include <iostream>
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
