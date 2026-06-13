#ifndef PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_SHIM_H
#define PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_SHIM_H

#include "../LanguageTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace PickVM {
    class Runtime;
}

namespace PickCore::Languages {
    class LanguageRegistry;

    namespace Basic {
        [[nodiscard]] std::optional<FunctionId> functionIdForBuiltinName(const std::string &name, int stackSize);

        void invokeBuiltinShim(const std::string &name,
                               std::vector<PickVM::Value> &stack,
                               PickVM::Runtime *rt,
                               const LanguageRegistry *registry);
    } // namespace Basic
} // namespace PickCore::Languages

#endif // PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_SHIM_H
