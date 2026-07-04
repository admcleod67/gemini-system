#include <doctest/doctest.h>

#include <pick_system/version.hpp>

#include "GeminiSession.h"
#include "Runtime.h"

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
