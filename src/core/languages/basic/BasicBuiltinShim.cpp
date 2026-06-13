#include "BasicBuiltinShim.h"

#include "BasicBuiltinLookup.h"
#include "BasicLanguageIds.h"
#include "LanguageRegistry.h"
#include "Runtime.h"

#include <stdexcept>
#include <string>

namespace PickCore::Languages::Basic {
    namespace {
        [[nodiscard]] int argCountForBuiltinName(const std::string &name, const int stackSize) {
            if (name == "RND") {
                return stackSize;
            }
            if (name == "DATE" || name == "TIME" || name == "STATUS") {
                return 0;
            }
            if (name == "MOD") {
                return 2;
            }
            if (name == "INDEX" || name == "FIELD" || name == "CONVERT") {
                return 3;
            }
            if (name == "OCONV" || name == "ICONV") {
                return 2;
            }
            return 1;
        }
    } // namespace

    std::optional<FunctionId> functionIdForBuiltinName(const std::string &name, const int stackSize) {
        if (name == "RND") {
            if (stackSize < 0 || stackSize > 1) {
                return std::nullopt;
            }
            return stackSize == 0 ? kFnRnd0 : kFnRnd1;
        }
        return functionIdForCall(name, argCountForBuiltinName(name, stackSize));
    }

    void invokeBuiltinShim(const std::string &name,
                           std::vector<PickVM::Value> &stack,
                           PickVM::Runtime *const rt,
                           const LanguageRegistry *const registry) {
        if (registry == nullptr) {
            throw std::runtime_error("BUILTIN: registry not configured");
        }

        const int stackSize = static_cast<int>(stack.size());
        const std::optional<FunctionId> fnId = functionIdForBuiltinName(name, stackSize);
        if (!fnId.has_value()) {
            throw std::runtime_error(std::string("BUILTIN: unknown ") + name);
        }

        const int argCount = argCountForBuiltinName(name, stackSize);
        if (name == "RND") {
            if (stackSize > 1) {
                throw std::runtime_error("BUILTIN: RND too many arguments");
            }
        } else if (stackSize < argCount) {
            throw std::runtime_error(std::string("BUILTIN: stack underflow for ") + name);
        }

        registry->dispatch(kNamespaceId, *fnId, stack, argCount, rt);
    }
} // namespace PickCore::Languages::Basic
