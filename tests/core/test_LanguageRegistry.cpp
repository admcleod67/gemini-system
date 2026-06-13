#include <doctest/doctest.h>

#include "LanguageRegistry.h"
#include "TestLanguageNamespace.h"

#include <functional>
#include <stdexcept>
#include <string>

using PickCore::Languages::FunctionId;
using PickCore::Languages::LanguageFunctionEntry;
using PickCore::Languages::LanguageNamespaceDescriptor;
using PickCore::Languages::LanguageRegistry;
using PickCore::Languages::NamespaceId;
using PickSystemTest::kFnAddOne;
using PickSystemTest::kFnEchoInt;
using PickSystemTest::kFnNoOp;
using PickSystemTest::kTestNamespaceId;
using PickSystemTest::makeTestNamespaceDescriptor;
using PickSystemTest::registerTestNamespace;
using PickVM::Value;

namespace {
    bool throwsWithMessage(const std::function<void()> &fn, const std::string &substring) {
        try {
            fn();
        } catch (const std::runtime_error &error) {
            return std::string(error.what()).find(substring) != std::string::npos;
        }
        return false;
    }
} // namespace

TEST_CASE("LanguageRegistry register namespace and dispatch happy path") {
    LanguageRegistry registry;
    registerTestNamespace(registry);

    const auto meta = registry.metadata(kTestNamespaceId);
    REQUIRE(meta.has_value());
    CHECK(meta->name == "test");
    CHECK(meta->version == "1");
    CHECK(registry.namespaceCount() == 1);

    std::vector<Value> stack{42};
    registry.dispatch(kTestNamespaceId, kFnEchoInt, stack, 1);
    REQUIRE(stack.size() == 1);
    CHECK(std::get<int>(stack.back()) == 42);

    stack = {7};
    registry.dispatch(kTestNamespaceId, kFnAddOne, stack, 1);
    REQUIRE(stack.size() == 1);
    CHECK(std::get<int>(stack.back()) == 8);

    stack.clear();
    registry.dispatch(kTestNamespaceId, kFnNoOp, stack, 0);
    REQUIRE(stack.size() == 1);
    CHECK(std::get<std::string>(stack.back()) == "ok");
}

TEST_CASE("LanguageRegistry unknown namespace") {
    LanguageRegistry registry;
    registerTestNamespace(registry);

    std::vector<Value> stack{1};
    CHECK(throwsWithMessage(
        [&] { registry.dispatch(0x99999999, kFnEchoInt, stack, 1); }, "LANG: unknown namespace"));
}

TEST_CASE("LanguageRegistry unknown function ID") {
    LanguageRegistry registry;
    registerTestNamespace(registry);

    std::vector<Value> stack{1};
    CHECK(throwsWithMessage(
        [&] { registry.dispatch(kTestNamespaceId, 99, stack, 1); }, "LANG: unknown function"));
}

TEST_CASE("LanguageRegistry unknown function slot with null handler") {
    LanguageRegistry registry;
    auto descriptor = makeTestNamespaceDescriptor();
    descriptor.functions.push_back(LanguageFunctionEntry{0, nullptr});
    registry.registerNamespace(std::move(descriptor));

    std::vector<Value> stack;
    CHECK(throwsWithMessage(
        [&] { registry.dispatch(kTestNamespaceId, 3, stack, 0); }, "LANG: unknown function"));
}

TEST_CASE("LanguageRegistry arity mismatch wrong argCount") {
    LanguageRegistry registry;
    registerTestNamespace(registry);

    std::vector<Value> stack{1, 2};
    CHECK(throwsWithMessage(
        [&] { registry.dispatch(kTestNamespaceId, kFnEchoInt, stack, 2); }, "LANG: arity mismatch"));
}

TEST_CASE("LanguageRegistry arity mismatch stack underflow") {
    LanguageRegistry registry;
    registerTestNamespace(registry);

    std::vector<Value> stack;
    CHECK(throwsWithMessage(
        [&] { registry.dispatch(kTestNamespaceId, kFnEchoInt, stack, 1); }, "LANG: arity mismatch"));
}

TEST_CASE("LanguageRegistry duplicate namespace ID on register") {
    LanguageRegistry registry;
    registerTestNamespace(registry);

    CHECK(throwsWithMessage([&] { registerTestNamespace(registry); }, "LANG: duplicate namespace"));
}

TEST_CASE("LanguageRegistry register after freeze") {
    LanguageRegistry registry;
    registerTestNamespace(registry);
    registry.freeze();
    CHECK(registry.isFrozen());

    CHECK(throwsWithMessage([&] { registerTestNamespace(registry); }, "LANG: registry frozen"));
}

TEST_CASE("LanguageRegistry freeze then dispatch still works") {
    LanguageRegistry registry;
    registerTestNamespace(registry);
    registry.freeze();

    std::vector<Value> stack{10};
    registry.dispatch(kTestNamespaceId, kFnAddOne, stack, 1);
    REQUIRE(stack.size() == 1);
    CHECK(std::get<int>(stack.back()) == 11);
}

TEST_CASE("LanguageRegistry onInit hook at registration") {
    int initCount = 0;
    LanguageRegistry registry;

    struct InitState {
        int *counter;
    };
    InitState state{&initCount};

    auto descriptor = makeTestNamespaceDescriptor();
    descriptor.hooks.onInit = [](void *hostContext) {
        auto *initState = static_cast<InitState *>(hostContext);
        ++(*initState->counter);
    };

    registry.registerNamespace(std::move(descriptor), &state);
    CHECK(initCount == 1);
}
