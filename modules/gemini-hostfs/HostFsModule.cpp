#include "LanguageModuleAbi.h"
#include "LanguageRegistry.h"
#include "LanguageTypes.h"
#include "Runtime.h"

#include <gemini/HostFilesystem.hpp>
#include <gemini/hostfs_function_ids.hpp>
#include <gemini/namespace_ids.hpp>

#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {
    PickVM::Value popOne(std::vector<PickVM::Value> &stack) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        return v;
    }

    PickVM::Runtime *runtimeFromHost(void *const hostContext) {
        return static_cast<PickVM::Runtime *>(hostContext);
    }

    Gemini::HostFilesystem &requireHostFs(void *const hostContext) {
        PickVM::Runtime *const rt = runtimeFromHost(hostContext);
        if (rt == nullptr || rt->hostFilesystem() == nullptr) {
            throw std::runtime_error("HOSTFS: unbound");
        }
        return *rt->hostFilesystem();
    }

    std::string requireString(const PickVM::Value &v) {
        if (!std::holds_alternative<std::string>(v)) {
            throw std::runtime_error("HOSTFS: invalid path");
        }
        return std::get<std::string>(v);
    }

    int requireInt(const PickVM::Value &v) {
        if (std::holds_alternative<int>(v)) {
            return std::get<int>(v);
        }
        if (std::holds_alternative<double>(v)) {
            return static_cast<int>(std::get<double>(v));
        }
        throw std::runtime_error("HOSTFS: bad handle");
    }

    void handlerOpenRead(std::vector<PickVM::Value> &stack, void *const hostContext) {
        Gemini::HostFilesystem &fs = requireHostFs(hostContext);
        const std::string path = requireString(popOne(stack));
        stack.push_back(fs.openRead(path));
    }

    void handlerOpenWrite(std::vector<PickVM::Value> &stack, void *const hostContext) {
        Gemini::HostFilesystem &fs = requireHostFs(hostContext);
        const std::string path = requireString(popOne(stack));
        stack.push_back(fs.openWrite(path));
    }

    void handlerOpenAppend(std::vector<PickVM::Value> &stack, void *const hostContext) {
        Gemini::HostFilesystem &fs = requireHostFs(hostContext);
        const std::string path = requireString(popOne(stack));
        stack.push_back(fs.openAppend(path));
    }

    void handlerRead(std::vector<PickVM::Value> &stack, void *const hostContext) {
        Gemini::HostFilesystem &fs = requireHostFs(hostContext);
        // Arity 2: handle, maxBytes (maxBytes on top)
        const int maxBytes = requireInt(popOne(stack));
        const int handle = requireInt(popOne(stack));
        stack.push_back(fs.read(handle, maxBytes));
    }

    void handlerWrite(std::vector<PickVM::Value> &stack, void *const hostContext) {
        Gemini::HostFilesystem &fs = requireHostFs(hostContext);
        // Arity 2: handle, data (data on top)
        const PickVM::Value dataVal = popOne(stack);
        const int handle = requireInt(popOne(stack));
        if (!std::holds_alternative<std::string>(dataVal)) {
            throw std::runtime_error("HOSTFS: write failed");
        }
        stack.push_back(fs.write(handle, std::get<std::string>(dataVal)));
    }

    void handlerClose(std::vector<PickVM::Value> &stack, void *const hostContext) {
        Gemini::HostFilesystem &fs = requireHostFs(hostContext);
        const int handle = requireInt(popOne(stack));
        fs.close(handle);
        stack.push_back(0);
    }
} // namespace

GEMINI_LANGUAGE_MODULE_EXPORT void register_language(PickCore::Languages::LanguageRegistry &registry) {
    PickCore::Languages::LanguageNamespaceDescriptor descriptor{};
    descriptor.id = Gemini::kNamespaceIdHostFs;
    descriptor.metadata.name = "hostfs";
    descriptor.metadata.version = "1";
    descriptor.functions = {
        PickCore::Languages::LanguageFunctionEntry{1, handlerOpenRead},   // kFnOpenRead
        PickCore::Languages::LanguageFunctionEntry{1, handlerOpenWrite},  // kFnOpenWrite
        PickCore::Languages::LanguageFunctionEntry{1, handlerOpenAppend}, // kFnOpenAppend
        PickCore::Languages::LanguageFunctionEntry{2, handlerRead},       // kFnRead
        PickCore::Languages::LanguageFunctionEntry{2, handlerWrite},      // kFnWrite
        PickCore::Languages::LanguageFunctionEntry{1, handlerClose},      // kFnClose
    };
    static_assert(Gemini::HostFs::kFunctionCount == 6);
    registry.registerNamespace(std::move(descriptor));
}
