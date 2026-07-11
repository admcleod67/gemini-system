#include "DaemonConfig.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace PickCore {
    namespace {
        std::string trim(std::string text) {
            const auto isSpace = [](const unsigned char ch) { return std::isspace(ch) != 0; };
            while (!text.empty() && isSpace(static_cast<unsigned char>(text.front()))) {
                text.erase(text.begin());
            }
            while (!text.empty() && isSpace(static_cast<unsigned char>(text.back()))) {
                text.pop_back();
            }
            return text;
        }

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

        void applyEnvOverrides(DaemonConfig &config) {
            applyHostPathEnvOverrides(config.hostPaths);
            if (const auto maxSessions = sizeFromEnv("GEMINI_MAX_SESSIONS")) {
                config.maxSessions = *maxSessions;
            }
            if (const char *const socketEnv = std::getenv("GEMINI_DAEMON_SOCKET")) {
                if (socketEnv[0] != '\0') {
                    config.socketPath = socketEnv;
                }
            }
        }

        std::optional<std::string> applyConfigKey(DaemonConfig &config, const std::string &key,
                                                  const std::string &value) {
            if (key == "socket") {
                if (value.empty()) {
                    return "config key \"socket\" requires a non-empty value";
                }
                config.socketPath = value;
                return std::nullopt;
            }
            if (key == "max_sessions") {
                if (const auto parsed = parsePositiveSize(value.c_str())) {
                    config.maxSessions = *parsed;
                    return std::nullopt;
                }
                return "config key \"max_sessions\" requires a positive integer";
            }
            if (key == "pick_root") {
                if (value.empty()) {
                    return "config key \"pick_root\" requires a non-empty value";
                }
                config.hostPaths.pickFilesystemRoot = value;
                return std::nullopt;
            }
            if (key == "catalog_root") {
                if (value.empty()) {
                    return "config key \"catalog_root\" requires a non-empty value";
                }
                config.hostPaths.geminiCatalogRoot = value;
                return std::nullopt;
            }
            if (key == "modules_root") {
                if (value.empty()) {
                    return "config key \"modules_root\" requires a non-empty value";
                }
                config.hostPaths.geminiModulesRoot = value;
                return std::nullopt;
            }
            return "unknown config key \"" + key + "\"";
        }
    } // namespace

    void printDaemonUsage(const char *const programName) {
        const char *name = programName != nullptr ? programName : "gemini-daemon";
        std::cout << "Usage: " << name << " [options]\n"
                  << "Options:\n"
                  << "  --config PATH           Load key=value settings from a config file\n"
                  << "  --socket PATH           Unix domain socket path\n"
                  << "  --max-sessions N        Maximum concurrent session objects\n"
                  << "  --pick-root PATH        Pick account filesystem root\n"
                  << "  --catalog-root PATH     Gemini catalogue root\n"
                  << "  --modules-root PATH     Language modules directory\n"
                  << "  --help                  Show this help\n"
                  << "Environment:\n"
                  << "  GEMINI_DAEMON_CONFIG    Config file path (overridden by --config)\n"
                  << "  GEMINI_DAEMON_SOCKET    Default socket path\n"
                  << "  GEMINI_MAX_SESSIONS     Default session limit\n"
                  << "  GEMINI_FILESYSTEM_ROOT  Pick account root\n"
                  << "  GEMINI_CATALOG_ROOT     Catalogue root\n"
                  << "  GEMINI_MODULES_PATH     Modules directory\n"
                  << "Config file keys: socket, max_sessions, pick_root, catalog_root, modules_root\n"
                  << "Precedence: CLI > environment > config file > defaults\n";
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

    std::optional<std::string> loadDaemonConfigFile(const std::filesystem::path &path, DaemonConfig &config) {
        std::ifstream input(path);
        if (!input) {
            return "cannot open config file \"" + path.string() + "\"";
        }

        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            const auto hash = line.find('#');
            if (hash != std::string::npos) {
                line.resize(hash);
            }
            line = trim(std::move(line));
            if (line.empty()) {
                continue;
            }

            const auto eq = line.find('=');
            if (eq == std::string::npos) {
                return "config file \"" + path.string() + "\" line " + std::to_string(lineNumber) +
                       ": expected key=value";
            }

            const std::string key = trim(line.substr(0, eq));
            const std::string value = trim(line.substr(eq + 1));
            if (key.empty()) {
                return "config file \"" + path.string() + "\" line " + std::to_string(lineNumber) +
                       ": empty key";
            }
            if (const auto error = applyConfigKey(config, key, value)) {
                return "config file \"" + path.string() + "\" line " + std::to_string(lineNumber) + ": " + *error;
            }
        }
        return std::nullopt;
    }

    DaemonConfigResolution resolveDaemonConfig(const int argc, char *argv[]) {
        DaemonConfigResolution resolution{};
        resolution.config.hostPaths = resolveDefaultHostPaths();
        resolution.config.socketPath = defaultDaemonSocketPath();

        std::optional<std::filesystem::path> configPathFromCli;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                resolution.showHelp = true;
                return resolution;
            }
            if (arg == "--config") {
                if (i + 1 >= argc) {
                    resolution.error = "--config requires a path argument";
                    return resolution;
                }
                configPathFromCli = argv[++i];
                continue;
            }
            if (arg == "--socket" || arg == "--max-sessions" || arg == "--pick-root" || arg == "--catalog-root" ||
                arg == "--modules-root") {
                if (i + 1 < argc) {
                    ++i;
                }
                continue;
            }
        }

        std::optional<std::filesystem::path> configPath = configPathFromCli;
        if (!configPath.has_value()) {
            if (const char *const envPath = std::getenv("GEMINI_DAEMON_CONFIG")) {
                if (envPath[0] != '\0') {
                    configPath = envPath;
                }
            }
        }

        if (configPath.has_value()) {
            if (const auto error = loadDaemonConfigFile(*configPath, resolution.config)) {
                resolution.error = error;
                return resolution;
            }
        }

        applyEnvOverrides(resolution.config);

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                ++i;
                continue;
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
