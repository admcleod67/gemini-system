#ifndef PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_REGISTRY_H
#define PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_REGISTRY_H

#include "LanguageTypes.h"

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace PickCore::Languages {
    class LanguageRegistry {
    public:
        [[nodiscard]] static LanguageRegistry &instance() {
            static LanguageRegistry registry;
            return registry;
        }

        void registerNamespace(LanguageNamespaceDescriptor descriptor, void *hostContext = nullptr);
        void freeze();
        [[nodiscard]] bool isFrozen() const { return frozen_; }

        void dispatch(NamespaceId nsId,
                        FunctionId fnId,
                        std::vector<PickVM::Value> &stack,
                        int argCount,
                        void *hostContext = nullptr) const;

        [[nodiscard]] std::optional<LanguageNamespaceMetadata> metadata(NamespaceId nsId) const;
        [[nodiscard]] std::size_t namespaceCount() const { return namespaces_.size(); }

    private:
        struct RegisteredNamespace {
            LanguageNamespaceMetadata metadata;
            std::vector<LanguageFunctionEntry> functions;
            LanguageNamespaceHooks hooks{};
        };

        std::unordered_map<NamespaceId, RegisteredNamespace> namespaces_;
        bool frozen_{false};
    };
} // namespace PickCore::Languages

#endif // PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_REGISTRY_H
