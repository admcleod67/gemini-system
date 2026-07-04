#include "GeminiServiceDaemon.h"

#include "BootMonitor.h"
#include "LanguageRegistry.h"
#include "LockRegistry.h"
#include "LockTable.h"

#include <Runtime.h>

namespace PickCore {
    GeminiServiceDaemon::GeminiServiceDaemon()
        : hostPaths_(resolveDefaultHostPaths()),
          lockTable_(std::make_shared<Locking::LockTable>()) {
        Locking::LockRegistry::adoptTable(lockTable_);
    }

    GeminiServiceDaemon GeminiServiceDaemon::createEmbedded() {
        return GeminiServiceDaemon{};
    }

    void GeminiServiceDaemon::coldStart(std::ostream &out) {
        if (coldStarted_) {
            return;
        }

        BootContext bootCtx;
        bootCtx.runtime = &bootstrapRuntime_;
        bootCtx.hostPaths = hostPaths_;
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
} // namespace PickCore
