#include "LanguageModuleAbi.h"
#include "LanguageRegistry.h"
#include "LanguageTypes.h"

#include <gemini/namespace_ids.hpp>

GEMINI_LANGUAGE_MODULE_EXPORT void register_language(PickCore::Languages::LanguageRegistry &registry) {
    PickCore::Languages::LanguageNamespaceDescriptor descriptor{};
    descriptor.id = Gemini::kNamespaceIdCobol;
    descriptor.metadata.name = "cobol";
    descriptor.metadata.version = "1";
    registry.registerNamespace(std::move(descriptor));
}
