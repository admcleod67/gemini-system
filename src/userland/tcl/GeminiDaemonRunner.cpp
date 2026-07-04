#include "GeminiDaemonRunner.h"

#include <pick_system/version.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <optional>
#include <thread>
#include <vector>

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

#ifndef _WIN32
    PickCore::AttachSessionResult GeminiDaemonRunner::attachSession(const PickCore::SessionId requestedPort,
                                                                    PickCore::IpcSessionChannel &channel) {
        PickCore::SessionId port = requestedPort;

        if (port == 0) {
            try {
                port = host_.createSession().id;
            } catch (const std::exception &) {
                return {PickCore::AttachSessionStatus::SessionTableFull, 0};
            }
        } else if (host_.sessions().find(port) == nullptr) {
            return {PickCore::AttachSessionStatus::SessionNotFound, 0};
        }

        if (boundSessionPorts_.contains(port)) {
            return {PickCore::AttachSessionStatus::SessionAlreadyBound, 0};
        }

        GeminiSession *session = host_.sessions().find(port);
        if (session == nullptr) {
            return {PickCore::AttachSessionStatus::SessionNotFound, 0};
        }

        session->setInputStream(&channel.input());
        session->setOutputStream(&channel.output());
        session->setDiagnosticStream(&channel.diagnostic());
        boundSessionPorts_[port] = 1;

        return {PickCore::AttachSessionStatus::Ok, port};
    }

    void GeminiDaemonRunner::detachSession(const PickCore::SessionId port) {
        boundSessionPorts_.erase(port);

        GeminiSession *session = host_.sessions().find(port);
        if (session == nullptr) {
            return;
        }

        session->setInputStream(nullptr);
        session->setOutputStream(nullptr);
        session->setDiagnosticStream(nullptr);
    }
#endif

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
        handlers.attachSession = [this](const PickCore::SessionId requestedPort,
                                        PickCore::IpcSessionChannel &channel) {
            return attachSession(requestedPort, channel);
        };
        handlers.detachSession = [this](const PickCore::SessionId port) { detachSession(port); };
        handlers.requestShutdown = [this] { requestShutdown(); };

        while (!shutdownRequested.load(std::memory_order_acquire)) {
            if (ipcServer_.pollAndDispatch(std::chrono::milliseconds(100), serverConfig, handlers)) {
                break;
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
        std::vector<PickCore::SessionId> boundPorts;
        boundPorts.reserve(boundSessionPorts_.size());
        for (const auto &entry : boundSessionPorts_) {
            boundPorts.push_back(entry.first);
        }
        for (const PickCore::SessionId port : boundPorts) {
            detachSession(port);
        }
        boundSessionPorts_.clear();
        ipcServer_.stop();
#endif
        host_.destroyAllSessions();
    }
} // namespace PickShell
