#ifndef PICK_SYSTEM_TESTS_CORE_TEST_LANGUAGE_NAMESPACE_H
#define PICK_SYSTEM_TESTS_CORE_TEST_LANGUAGE_NAMESPACE_H

#include "LanguageRegistry.h"

#include <vector>

namespace PickSystemTest {
    constexpr PickCore::Languages::NamespaceId kTestNamespaceId = 0x00000001;
    constexpr PickCore::Languages::FunctionId kFnEchoInt = 0;
    constexpr PickCore::Languages::FunctionId kFnAddOne = 1;
    constexpr PickCore::Languages::FunctionId kFnNoOp = 2;

    inline void fnEchoInt(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        const int value = std::get<int>(stack.back());
        stack.pop_back();
        stack.push_back(value);
    }

    inline void fnAddOne(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        const int value = std::get<int>(stack.back());
        stack.pop_back();
        stack.push_back(value + 1);
    }

    inline void fnNoOp(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        stack.push_back(std::string("ok"));
    }

    inline PickCore::Languages::LanguageNamespaceDescriptor makeTestNamespaceDescriptor() {
        PickCore::Languages::LanguageNamespaceDescriptor descriptor{};
        descriptor.id = kTestNamespaceId;
        descriptor.metadata.name = "test";
        descriptor.metadata.version = "1";
        descriptor.functions = {
            PickCore::Languages::LanguageFunctionEntry{1, fnEchoInt},
            PickCore::Languages::LanguageFunctionEntry{1, fnAddOne},
            PickCore::Languages::LanguageFunctionEntry{0, fnNoOp},
        };
        return descriptor;
    }

    inline void registerTestNamespace(PickCore::Languages::LanguageRegistry &registry) {
        registry.registerNamespace(makeTestNamespaceDescriptor());
    }
} // namespace PickSystemTest

#endif // PICK_SYSTEM_TESTS_CORE_TEST_LANGUAGE_NAMESPACE_H
