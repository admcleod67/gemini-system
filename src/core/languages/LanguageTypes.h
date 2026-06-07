#ifndef PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_TYPES_H
#define PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_TYPES_H

#include "Runtime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PickCore::Languages {
    using NamespaceId = std::uint32_t;
    using FunctionId = std::uint32_t;

    using LanguageFunctionFn = void (*)(std::vector<PickVM::Value> &stack, void *hostContext);

    struct LanguageFunctionEntry {
        int arity{0};
        LanguageFunctionFn fn{nullptr};
    };

    struct LanguageNamespaceMetadata {
        std::string name;
        std::string version;
    };

    struct LanguageNamespaceHooks {
        void (*onInit)(void *hostContext){nullptr};
        void (*onTeardown)(void *hostContext){nullptr};
    };

    struct LanguageNamespaceDescriptor {
        NamespaceId id{0};
        LanguageNamespaceMetadata metadata;
        std::vector<LanguageFunctionEntry> functions;
        LanguageNamespaceHooks hooks{};
    };

    [[noreturn]] void throwLanguageRegistryError(const char *kind, const std::string &detail = {});
} // namespace PickCore::Languages

#endif // PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_TYPES_H
