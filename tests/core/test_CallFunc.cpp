#include <doctest/doctest.h>

#include "Runtime.h"
#include "TestLanguageNamespace.h"

#include <cstdint>
#include <functional>
#include <sstream>

using PickCore::Languages::LanguageRegistry;
using PickSystemTest::kFnAddOne;
using PickSystemTest::kFnEchoInt;
using PickSystemTest::kFnNoOp;
using PickSystemTest::kTestNamespaceId;
using PickSystemTest::registerTestNamespace;
using PickVM::Instruction;
using PickVM::OpCode;
using PickVM::Runtime;
using PickVM::Value;

namespace {
    Instruction makeCallFunc(const std::uint32_t namespaceId,
                             const std::uint32_t functionId,
                             const int argCount) {
        Instruction instr{};
        instr.op = OpCode::CallFunc;
        instr.callFunc = {namespaceId, functionId, argCount};
        return instr;
    }

    bool throwsWithMessage(const std::function<void()> &fn, const std::string &substring) {
        try {
            fn();
        } catch (const std::runtime_error &error) {
            return std::string(error.what()).find(substring) != std::string::npos;
        }
        return false;
    }

    LanguageRegistry makeTestRegistry() {
        LanguageRegistry registry;
        registerTestNamespace(registry);
        return registry;
    }
} // namespace

TEST_CASE("runtime CALL_FUNC echo int via injected registry") {
    LanguageRegistry registry = makeTestRegistry();
    Runtime rt;
    rt.setLanguageRegistry(&registry);
    rt.loadProgram({
        {OpCode::PushInt, 42},
        makeCallFunc(kTestNamespaceId, kFnEchoInt, 1),
        {OpCode::Halt, Value{}},
    });
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 42);
}

TEST_CASE("runtime CALL_FUNC add-one via injected registry") {
    LanguageRegistry registry = makeTestRegistry();
    Runtime rt;
    rt.setLanguageRegistry(&registry);
    rt.loadProgram({
        {OpCode::PushInt, 41},
        makeCallFunc(kTestNamespaceId, kFnAddOne, 1),
        {OpCode::Halt, Value{}},
    });
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 42);
}

TEST_CASE("runtime CALL_FUNC no-op pushes ok string") {
    LanguageRegistry registry = makeTestRegistry();
    Runtime rt;
    rt.setLanguageRegistry(&registry);
    rt.loadProgram({
        makeCallFunc(kTestNamespaceId, kFnNoOp, 0),
        {OpCode::Halt, Value{}},
    });
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "ok");
}

TEST_CASE("runtime CALL_FUNC prints result from text program") {
    LanguageRegistry registry = makeTestRegistry();
    Runtime rt;
    rt.setLanguageRegistry(&registry);
    std::ostringstream out;
    rt.setOutputStream(&out);
    rt.loadProgram({
        {OpCode::PushInt, 41},
        makeCallFunc(1, kFnAddOne, 1),
        {OpCode::PrintVal, Value{}},
        {OpCode::Halt, Value{}},
    });
    rt.run();
    CHECK(out.str() == "42");
}

TEST_CASE("runtime CALL_FUNC without registry throws LANG registry not configured") {
    Runtime rt;
    rt.loadProgram({
        {OpCode::PushInt, 1},
        makeCallFunc(1, 0, 1),
        {OpCode::Halt, Value{}},
    });
    CHECK(throwsWithMessage([&] { rt.run(); }, "LANG: registry not configured"));
}

TEST_CASE("runtime CALL_FUNC unknown namespace propagates LANG error") {
    LanguageRegistry registry = makeTestRegistry();
    Runtime rt;
    rt.setLanguageRegistry(&registry);
    rt.loadProgram({
        {OpCode::PushInt, 1},
        makeCallFunc(0x99999999, 0, 1),
        {OpCode::Halt, Value{}},
    });
    CHECK(throwsWithMessage([&] { rt.run(); }, "LANG: unknown namespace"));
}

TEST_CASE("runtime CALL_FUNC unknown function propagates LANG error") {
    LanguageRegistry registry = makeTestRegistry();
    Runtime rt;
    rt.setLanguageRegistry(&registry);
    rt.loadProgram({
        {OpCode::PushInt, 1},
        makeCallFunc(kTestNamespaceId, 99, 1),
        {OpCode::Halt, Value{}},
    });
    CHECK(throwsWithMessage([&] { rt.run(); }, "LANG: unknown function"));
}

TEST_CASE("runtime CALL_FUNC arity mismatch propagates LANG error") {
    LanguageRegistry registry = makeTestRegistry();
    Runtime rt;
    rt.setLanguageRegistry(&registry);
    rt.loadProgram({
        makeCallFunc(kTestNamespaceId, kFnEchoInt, 1),
        {OpCode::Halt, Value{}},
    });
    CHECK(throwsWithMessage([&] { rt.run(); }, "LANG: arity mismatch"));
}
