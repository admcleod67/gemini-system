#include "LanguageModuleAbi.h"
#include "LanguageRegistry.h"
#include "LanguageTypes.h"

#include <string>
#include <vector>

namespace {
    constexpr PickCore::Languages::NamespaceId kStubNamespaceId = 0x00000100;

    void stubNoOp(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        stack.push_back(std::string("stub-ok"));
    }
} // namespace

GEMINI_LANGUAGE_MODULE_EXPORT void register_language(PickCore::Languages::LanguageRegistry &registry) {
    PickCore::Languages::LanguageNamespaceDescriptor descriptor{};
    descriptor.id = kStubNamespaceId;
    descriptor.metadata.name = "stub";
    descriptor.metadata.version = "1";
    descriptor.functions = {
        PickCore::Languages::LanguageFunctionEntry{0, stubNoOp},
    };
    registry.registerNamespace(std::move(descriptor));
}
