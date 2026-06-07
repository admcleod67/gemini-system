#include "LanguageRegistry.h"

#include <stdexcept>
#include <utility>

namespace PickCore::Languages {
    namespace {
        [[noreturn]] void throwError(const char *kind, const std::string &detail = {}) {
            throwLanguageRegistryError(kind, detail);
        }
    } // namespace

    void throwLanguageRegistryError(const char *kind, const std::string &detail) {
        std::string message = std::string("LANG: ") + kind;
        if (!detail.empty()) {
            message += " (";
            message += detail;
            message += ')';
        }
        throw std::runtime_error(message);
    }

    void LanguageRegistry::registerNamespace(LanguageNamespaceDescriptor descriptor, void *const hostContext) {
        if (frozen_) {
            throwError("registry frozen");
        }
        if (namespaces_.find(descriptor.id) != namespaces_.end()) {
            throwError("duplicate namespace", std::to_string(descriptor.id));
        }

        RegisteredNamespace registered{};
        registered.metadata = std::move(descriptor.metadata);
        registered.functions = std::move(descriptor.functions);
        registered.hooks = descriptor.hooks;

        if (registered.hooks.onInit != nullptr) {
            registered.hooks.onInit(hostContext);
        }

        namespaces_.emplace(descriptor.id, std::move(registered));
    }

    void LanguageRegistry::freeze() {
        frozen_ = true;
    }

    void LanguageRegistry::dispatch(const NamespaceId nsId,
                                    const FunctionId fnId,
                                    std::vector<PickVM::Value> &stack,
                                    const int argCount,
                                    void *const hostContext) const {
        const auto nsIt = namespaces_.find(nsId);
        if (nsIt == namespaces_.end()) {
            throwError("unknown namespace", std::to_string(nsId));
        }

        const RegisteredNamespace &ns = nsIt->second;
        if (fnId >= ns.functions.size()) {
            throwError("unknown function", std::to_string(fnId));
        }

        const LanguageFunctionEntry &entry = ns.functions[fnId];
        if (entry.fn == nullptr) {
            throwError("unknown function", std::to_string(fnId));
        }

        if (argCount != entry.arity) {
            throwError("arity mismatch");
        }
        if (static_cast<int>(stack.size()) < argCount) {
            throwError("arity mismatch");
        }

        entry.fn(stack, hostContext);
    }

    std::optional<LanguageNamespaceMetadata> LanguageRegistry::metadata(const NamespaceId nsId) const {
        const auto it = namespaces_.find(nsId);
        if (it == namespaces_.end()) {
            return std::nullopt;
        }
        return it->second.metadata;
    }
} // namespace PickCore::Languages
