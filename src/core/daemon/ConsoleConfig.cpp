#include "ConsoleConfig.h"

#include "DaemonConfig.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace PickCore {
    namespace {
        [[nodiscard]] std::optional<SessionId> parsePort(const char *text) {
            if (text == nullptr || text[0] == '\0') {
                return std::nullopt;
            }
            char *end = nullptr;
            const unsigned long value = std::strtoul(text, &end, 10);
            if (end == text || *end != '\0') {
                return std::nullopt;
            }
            return static_cast<SessionId>(value);
        }
    } // namespace

    void printConsoleUsage(const char *const programName) {
        const char *name = programName != nullptr ? programName : "gemini-console";
        std::cout << "Usage: " << name << " [options]\n"
                  << "Options:\n"
                  << "  --socket PATH     Daemon Unix domain socket path\n"
                  << "  --port N          Attach to existing session port (omit to create)\n"
                  << "  --help            Show this help\n"
                  << "Environment:\n"
                  << "  GEMINI_DAEMON_SOCKET   Default socket path\n";
    }

    ConsoleConfigResolution resolveConsoleConfig(const int argc, char *argv[]) {
        ConsoleConfigResolution resolution{};
        resolution.config.socketPath = defaultDaemonSocketPath();

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
            if (arg == "--port" && i + 1 < argc) {
                if (const std::optional<SessionId> port = parsePort(argv[++i])) {
                    resolution.config.requestedPort = *port;
                }
                continue;
            }
        }

        return resolution;
    }
} // namespace PickCore
