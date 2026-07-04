#include "GeminiDaemonRunner.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>

namespace PickShell {
    namespace {
        std::atomic<bool> shutdownRequested{false};

        void handleTerminationSignal(const int /*signal*/) {
            shutdownRequested.store(true, std::memory_order_release);
        }
    } // namespace

    GeminiDaemonRunner::GeminiDaemonRunner(PickCore::GeminiServiceDaemon &daemon,
                                           GeminiSessionHost &host,
                                           const PickCore::DaemonConfig &config)
        : daemon_(daemon), host_(host), config_(config) {}

    void GeminiDaemonRunner::requestShutdown() {
        shutdownRequested.store(true, std::memory_order_release);
    }

    void GeminiDaemonRunner::installSignalHandlers() {
        shutdownRequested.store(false, std::memory_order_release);
        std::signal(SIGTERM, handleTerminationSignal);
        std::signal(SIGINT, handleTerminationSignal);
    }

    int GeminiDaemonRunner::run(std::ostream &bootOut) {
        installSignalHandlers();
        daemon_.coldStart(bootOut);

        while (!shutdownRequested.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        shutdown();
        return 0;
    }

    void GeminiDaemonRunner::shutdown() {
        host_.destroyAllSessions();

        std::error_code ec;
        if (std::filesystem::exists(config_.socketPath, ec) && !ec) {
            std::filesystem::remove(config_.socketPath, ec);
        }
    }
} // namespace PickShell
