#include "GeminiServiceDaemon.h"

#include "BootMonitor.h"
#include "LanguageRegistry.h"
#include "LockRegistry.h"
#include "LockTable.h"
#include "PortManager.h"

#include <Runtime.h>

namespace PickCore {
    GeminiServiceDaemon::GeminiServiceDaemon(const DaemonConfig &config)
        : maxSessions_(config.maxSessions < 1 ? 1 : config.maxSessions),
          hostPaths_(config.hostPaths),
          lockTable_(std::make_shared<Locking::LockTable>()),
          portManager_(maxSessions_) {
        Locking::LockRegistry::adoptTable(lockTable_);
    }

    GeminiServiceDaemon GeminiServiceDaemon::createEmbedded() {
        return GeminiServiceDaemon(DaemonConfig::embedded());
    }

    GeminiServiceDaemon GeminiServiceDaemon::create(const DaemonConfig &config) {
        return GeminiServiceDaemon(config);
    }

    void GeminiServiceDaemon::coldStart(std::ostream &out) {
        if (coldStarted_) {
            return;
        }

        BootContext bootCtx;
        bootCtx.runtime = &bootstrapRuntime_;
        bootCtx.hostPaths = hostPaths_;
        bootCtx.portManager = &portManager_;
        BootMonitor::runColdStart(out, bootCtx);
        coldStarted_ = true;
    }

    Languages::LanguageRegistry &GeminiServiceDaemon::languageRegistry() {
        return Languages::LanguageRegistry::instance();
    }

    const Languages::LanguageRegistry &GeminiServiceDaemon::languageRegistry() const {
        return Languages::LanguageRegistry::instance();
    }

    std::shared_ptr<Locking::LockTable> GeminiServiceDaemon::lockTable() const {
        return lockTable_;
    }

    PortManager &GeminiServiceDaemon::portManager() {
        return portManager_;
    }

    const PortManager &GeminiServiceDaemon::portManager() const {
        return portManager_;
    }
} // namespace PickCore
