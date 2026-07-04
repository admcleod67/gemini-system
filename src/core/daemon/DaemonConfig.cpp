#include "DaemonConfig.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace PickCore {
    namespace {
        std::optional<std::size_t> parsePositiveSize(const char *text) {
            if (text == nullptr || text[0] == '\0') {
                return std::nullopt;
            }
            char *end = nullptr;
            const unsigned long value = std::strtoul(text, &end, 10);
            if (end == text || *end != '\0' || value < 1) {
                return std::nullopt;
            }
            return static_cast<std::size_t>(value);
        }

        std::optional<std::size_t> sizeFromEnv(const char *name) {
            if (const char *const value = std::getenv(name)) {
                return parsePositiveSize(value);
            }
            return std::nullopt;
        }

        void applyHostPathEnvOverrides(DefaultHostPaths &paths) {
            if (const char *const value = std::getenv("GEMINI_FILESYSTEM_ROOT")) {
                if (value[0] != '\0') {
                    paths.pickFilesystemRoot = std::filesystem::path(value);
                }
            }
            if (const char *const value = std::getenv("GEMINI_CATALOG_ROOT")) {
                if (value[0] != '\0') {
                    paths.geminiCatalogRoot = std::filesystem::path(value);
                }
            }
            if (const char *const value = std::getenv("GEMINI_MODULES_PATH")) {
                if (value[0] != '\0') {
                    paths.geminiModulesRoot = std::filesystem::path(value);
                }
            }
        }

    } // namespace

    void printDaemonUsage(const char *const programName) {
        const char *name = programName != nullptr ? programName : "gemini-daemon";
        std::cout << "Usage: " << name << " [options]\n"
                  << "Options:\n"
                  << "  --socket PATH           Unix domain socket path\n"
                  << "  --max-sessions N        Maximum concurrent session objects\n"
                  << "  --pick-root PATH        Pick account filesystem root\n"
                  << "  --catalog-root PATH     Gemini catalogue root\n"
                  << "  --modules-root PATH     Language modules directory\n"
                  << "  --help                  Show this help\n"
                  << "Environment:\n"
                  << "  GEMINI_DAEMON_SOCKET    Default socket path\n"
                  << "  GEMINI_MAX_SESSIONS     Default session limit\n"
                  << "  GEMINI_FILESYSTEM_ROOT  Pick account root\n"
                  << "  GEMINI_CATALOG_ROOT     Catalogue root\n"
                  << "  GEMINI_MODULES_PATH     Modules directory\n";
    }

    std::filesystem::path defaultDaemonSocketPath() {
        if (const char *const runtimeDir = std::getenv("XDG_RUNTIME_DIR")) {
            if (runtimeDir[0] != '\0') {
                return std::filesystem::path(runtimeDir) / "gemini.sock";
            }
        }
        return std::filesystem::path("/tmp/gemini.sock");
    }

    DaemonConfig DaemonConfig::embedded() {
        DaemonConfig config{};
        config.maxSessions = 1;
        config.hostPaths = resolveDefaultHostPaths();
        return config;
    }

    DaemonConfigResolution resolveDaemonConfig(const int argc, char *argv[]) {
        DaemonConfigResolution resolution{};
        resolution.config.hostPaths = resolveDefaultHostPaths();
        resolution.config.socketPath = defaultDaemonSocketPath();
        applyHostPathEnvOverrides(resolution.config.hostPaths);

        if (const auto maxSessions = sizeFromEnv("GEMINI_MAX_SESSIONS")) {
            resolution.config.maxSessions = *maxSessions;
        }
        if (const char *const socketEnv = std::getenv("GEMINI_DAEMON_SOCKET")) {
            if (socketEnv[0] != '\0') {
                resolution.config.socketPath = socketEnv;
            }
        }

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                resolution.showHelp = true;
                return resolution;
            }
            if (arg == "--socket" && i + 1 < argc) {
                resolution.config.socketPath = argv[++i];
                continue;
            }
            if (arg == "--max-sessions" && i + 1 < argc) {
                if (const auto parsed = parsePositiveSize(argv[++i])) {
                    resolution.config.maxSessions = *parsed;
                }
                continue;
            }
            if (arg == "--pick-root" && i + 1 < argc) {
                resolution.config.hostPaths.pickFilesystemRoot = argv[++i];
                continue;
            }
            if (arg == "--catalog-root" && i + 1 < argc) {
                resolution.config.hostPaths.geminiCatalogRoot = argv[++i];
                continue;
            }
            if (arg == "--modules-root" && i + 1 < argc) {
                resolution.config.hostPaths.geminiModulesRoot = argv[++i];
                continue;
            }
        }

        if (resolution.config.maxSessions < 1) {
            resolution.config.maxSessions = 1;
        }
        return resolution;
    }
} // namespace PickCore
