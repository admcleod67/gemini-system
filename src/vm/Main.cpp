#include <LanguageModuleLoader.h>
#include <LanguageRegistry.h>

#include <pickvm/core.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {
    void printUsage(const char *argv0) {
        const char *const name = argv0 != nullptr ? argv0 : "gemini-vm";
        std::cout << "Usage: " << name << " [--modules PATH] [-h|--help] <program.tbc>\n"
                  << "\n"
                  << "Run a Gemini text bytecode (.tbc) file with console I/O.\n"
                  << "Does not attach Pick catalogue, Tcl, session table, or filesystem.\n"
                  << "\n"
                  << "Options:\n"
                  << "  --modules PATH   Directory of language modules (shared libraries)\n"
                  << "  -h, --help       Show this help and exit\n"
                  << "\n"
                  << "Modules path resolution (first match wins):\n"
                  << "  1. --modules PATH\n"
                  << "  2. GEMINI_MODULES_PATH environment variable\n"
                  << "  3. ./gemini/modules (relative to current working directory)\n";
    }

    std::filesystem::path resolveModulesDir(const std::string &modulesFlag) {
        if (!modulesFlag.empty()) {
            return std::filesystem::path(modulesFlag);
        }
        if (const char *const env = std::getenv("GEMINI_MODULES_PATH");
            env != nullptr && env[0] != '\0') {
            return std::filesystem::path(env);
        }
        return std::filesystem::path("gemini") / "modules";
    }
} // namespace

int main(const int argc, char *argv[]) {
    std::string modulesFlag;
    std::string programPath;
    bool showHelp = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i] != nullptr ? argv[i] : "";
        if (arg == "-h" || arg == "--help") {
            showHelp = true;
            continue;
        }
        if (arg == "--modules") {
            if (i + 1 >= argc || argv[i + 1] == nullptr || argv[i + 1][0] == '\0') {
                std::cerr << "gemini-vm: --modules requires a path argument\n";
                return 1;
            }
            modulesFlag = argv[++i];
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "gemini-vm: unknown option: " << arg << '\n';
            return 1;
        }
        if (!programPath.empty()) {
            std::cerr << "gemini-vm: unexpected argument: " << arg << '\n';
            return 1;
        }
        programPath.assign(arg);
    }

    if (showHelp) {
        printUsage(argc > 0 ? argv[0] : "gemini-vm");
        return 0;
    }

    if (programPath.empty()) {
        std::cerr << "gemini-vm: missing program path\n";
        printUsage(argc > 0 ? argv[0] : "gemini-vm");
        return 1;
    }

    try {
        auto &registry = PickCore::Languages::LanguageRegistry::instance();
        const std::filesystem::path modulesDir = resolveModulesDir(modulesFlag);
        (void) PickCore::Languages::loadLanguageModules(registry, modulesDir, nullptr, nullptr);

        PickVM::Runtime runtime;
        runtime.setLanguageRegistry(&registry);

        PickVM::Parser parser;
        const PickVM::LoadedBytecode loaded = parser.parseFile(programPath);
        runtime.loadProgram(loaded.program, loaded.sourceLinePerInstr);
        runtime.run();
        return 0;
    } catch (const PickVM::UserInterrupt &) {
        std::cerr << "gemini-vm: interrupted\n";
        return 130;
    } catch (const PickVM::ChainRequest &req) {
        std::cerr << "gemini-vm: CHAIN not supported in standalone runner";
        if (!req.programName.empty()) {
            std::cerr << " (requested: " << req.programName << ')';
        }
        std::cerr << '\n';
        return 1;
    } catch (const std::exception &error) {
        std::cerr << "gemini-vm: " << error.what() << '\n';
        return 1;
    }
}
