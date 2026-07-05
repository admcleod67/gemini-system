//
// gemini-console configuration (Milestone 14 Stage 4).
//

#ifndef PICK_SYSTEM_CORE_DAEMON_CONSOLE_CONFIG_H
#define PICK_SYSTEM_CORE_DAEMON_CONSOLE_CONFIG_H

#include "SerialSessionRunner.h"

#include <filesystem>

namespace PickCore {
    struct ConsoleConfig {
        std::filesystem::path socketPath;
        SessionId requestedPort{0};
    };

    struct ConsoleConfigResolution {
        ConsoleConfig config;
        bool showHelp{false};
    };

    [[nodiscard]] ConsoleConfigResolution resolveConsoleConfig(int argc, char *argv[]);
    void printConsoleUsage(const char *programName);
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_CONSOLE_CONFIG_H
