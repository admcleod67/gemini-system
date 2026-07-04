#include "GeminiDaemonRunner.h"

#include <pick_system/version.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <optional>
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
        : daemon_(daemon),
          host_(host),
          config_(config)
#ifndef _WIN32
          ,
          ipcServer_(config.socketPath)
#endif
    {
    }

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

#ifndef _WIN32
        ipcServer_.start();

        PickCore::DaemonIpcServerConfig serverConfig{};
        serverConfig.maxSessions = config_.maxSessions;
        serverConfig.buildVersion = pick_system::version_string;

        PickCore::DaemonIpcHandlers handlers{};
        handlers.reserveSession = [this]() -> std::optional<PickCore::SessionId> {
            try {
                return host_.createSession().id;
            } catch (const std::exception &) {
                return std::nullopt;
            }
        };
        handlers.requestShutdown = [this] { requestShutdown(); };

        while (!shutdownRequested.load(std::memory_order_acquire)) {
            if (ipcServer_.pollAccept(std::chrono::milliseconds(100))) {
                if (ipcServer_.handleConnectedClient(serverConfig, handlers)) {
                    break;
                }
            }
        }
#else
        while (!shutdownRequested.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
#endif

        shutdown();
        return 0;
    }

    void GeminiDaemonRunner::shutdown() {
#ifndef _WIN32
        ipcServer_.stop();
#endif
        host_.destroyAllSessions();
    }
} // namespace PickShell
