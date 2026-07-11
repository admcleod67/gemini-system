//
// Gemini Service Daemon configuration (Milestone 13 Stage 4; M17 Stage 1 file config).
//

#ifndef PICK_SYSTEM_CORE_DAEMON_DAEMON_CONFIG_H
#define PICK_SYSTEM_CORE_DAEMON_DAEMON_CONFIG_H

#include "HostBootstrap.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace PickCore {
    struct DaemonConfig {
        std::filesystem::path socketPath;
        std::size_t maxSessions{64};
        DefaultHostPaths hostPaths;

        [[nodiscard]] static DaemonConfig embedded();
    };

    struct DaemonConfigResolution {
        DaemonConfig config;
        bool showHelp{false};
        std::optional<std::string> error;
    };

    [[nodiscard]] std::filesystem::path defaultDaemonSocketPath();
    [[nodiscard]] std::optional<std::string> loadDaemonConfigFile(const std::filesystem::path &path,
                                                                  DaemonConfig &config);
    [[nodiscard]] DaemonConfigResolution resolveDaemonConfig(int argc, char *argv[]);
    void printDaemonUsage(const char *programName);
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_DAEMON_CONFIG_H
