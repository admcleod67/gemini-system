#include "LanguageModuleLoader.h"

#include "LanguageModuleAbi.h"

#include <algorithm>
#include <exception>
#include <string>
#include <vector>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace PickCore::Languages {
    namespace {
#if defined(_WIN32)
        using ModuleHandle = HMODULE;

        ModuleHandle openModule(const std::filesystem::path &path, std::string &error) {
            const ModuleHandle handle = LoadLibraryW(path.c_str());
            if (handle == nullptr) {
                error = "failed to load";
            }
            return handle;
        }

        RegisterLanguageFn resolveRegisterLanguage(const ModuleHandle handle, std::string &error) {
            const auto symbol = reinterpret_cast<RegisterLanguageFn>(
                GetProcAddress(handle, kRegisterLanguageSymbol));
            if (symbol == nullptr) {
                error = "register_language not found";
            }
            return symbol;
        }

        void closeModule(const ModuleHandle handle) {
            if (handle != nullptr) {
                FreeLibrary(handle);
            }
        }
#else
        using ModuleHandle = void *;

        ModuleHandle openModule(const std::filesystem::path &path, std::string &error) {
            const ModuleHandle handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (handle == nullptr) {
                const char *const detail = dlerror();
                error = detail != nullptr ? detail : "failed to load";
            }
            return handle;
        }

        void closeModule(const ModuleHandle handle) {
            if (handle != nullptr) {
                dlclose(handle);
            }
        }

        RegisterLanguageFn resolveRegisterLanguage(const ModuleHandle handle, std::string &error) {
            dlerror();
            const auto symbol = reinterpret_cast<RegisterLanguageFn>(dlsym(handle, kRegisterLanguageSymbol));
            const char *const detail = dlerror();
            if (detail != nullptr) {
                error = detail;
                return nullptr;
            }
            if (symbol == nullptr) {
                error = "register_language not found";
            }
            return symbol;
        }
#endif

        void logLine(std::ostream *const log, const std::string &line) {
            if (log != nullptr) {
                (*log) << line << '\n';
            }
        }

        bool isSharedLibraryFile(const std::filesystem::path &path) {
            if (!path.has_filename()) {
                return false;
            }
            const std::string ext = path.extension().string();
#if defined(_WIN32)
            return ext == ".dll";
#else
            return ext == ".so" || ext == ".dylib";
#endif
        }

        std::vector<std::filesystem::path> listModuleFiles(const std::filesystem::path &modulesDir) {
            std::vector<std::filesystem::path> files;
            std::error_code ec;
            for (const auto &entry: std::filesystem::directory_iterator(modulesDir, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_regular_file(ec) || ec) {
                    continue;
                }
                if (isSharedLibraryFile(entry.path())) {
                    files.push_back(entry.path());
                }
            }
            std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
                return a.filename().string() < b.filename().string();
            });
            return files;
        }

        std::vector<ModuleHandle> &retainedModuleHandles() {
            static std::vector<ModuleHandle> handles;
            return handles;
        }
    } // namespace

    LanguageModuleLoadReport loadLanguageModules(LanguageRegistry &registry,
                                                 const std::filesystem::path &modulesDir,
                                                 std::ostream *const log,
                                                 void *const /*hostContext*/) {
        LanguageModuleLoadReport report{};

        std::error_code ec;
        if (!std::filesystem::is_directory(modulesDir, ec) || ec) {
            logLine(log, "MODULES: (skip — directory not present)");
            registry.freeze();
            return report;
        }

        logLine(log, "LOADING LANGUAGE MODULES...");

        const std::vector<std::filesystem::path> files = listModuleFiles(modulesDir);
        for (const auto &path: files) {
            ++report.attempted;
            const std::string fileName = path.filename().string();

            std::string openError;
            const ModuleHandle handle = openModule(path, openError);
            if (handle == nullptr) {
                ++report.failed;
                logLine(log, "MODULE " + fileName + ": (" + openError + ')');
                continue;
            }

            std::string symbolError;
            const RegisterLanguageFn registerLanguage = resolveRegisterLanguage(handle, symbolError);
            if (registerLanguage == nullptr) {
                ++report.failed;
                logLine(log, "MODULE " + fileName + ": (" + symbolError + ')');
                closeModule(handle);
                continue;
            }

            try {
                registerLanguage(registry);
            } catch (const std::exception &error) {
                ++report.failed;
                logLine(log, std::string("MODULE ") + fileName + ": (" + error.what() + ')');
                closeModule(handle);
                continue;
            }

            retainedModuleHandles().push_back(handle);
            ++report.loaded;
            logLine(log, "MODULE " + fileName + ": OK");
        }

        registry.freeze();
        return report;
    }
} // namespace PickCore::Languages
