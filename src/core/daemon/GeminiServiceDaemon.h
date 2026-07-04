//
// Process-scope Gemini Service Daemon host (Milestone 13).
// Owns cold start, frozen LanguageRegistry, and shared LockTable access.
//

#ifndef PICK_SYSTEM_CORE_DAEMON_GEMINI_SERVICE_DAEMON_H
#define PICK_SYSTEM_CORE_DAEMON_GEMINI_SERVICE_DAEMON_H

#include "HostBootstrap.h"
#include "PortManager.h"

#include <Runtime.h>

#include <iosfwd>
#include <memory>

namespace PickCore::Languages {
    class LanguageRegistry;
}

namespace PickCore::Locking {
    class LockTable;
}

namespace PickCore {
    /// Process host for embedded (maxSessions=1) and future long-running daemon modes.
    /// bootstrapRuntime_ is used only for BootContext during cold start; session VMs are separate.
    class GeminiServiceDaemon {
    public:
        [[nodiscard]] static GeminiServiceDaemon createEmbedded();

        void coldStart(std::ostream &out);
        [[nodiscard]] bool isColdStarted() const { return coldStarted_; }

        [[nodiscard]] Languages::LanguageRegistry &languageRegistry();
        [[nodiscard]] const Languages::LanguageRegistry &languageRegistry() const;
        [[nodiscard]] std::shared_ptr<Locking::LockTable> lockTable() const;
        [[nodiscard]] const DefaultHostPaths &hostPaths() const { return hostPaths_; }
        [[nodiscard]] PortManager &portManager();
        [[nodiscard]] const PortManager &portManager() const;

    private:
        GeminiServiceDaemon();

        DefaultHostPaths hostPaths_;
        PickVM::Runtime bootstrapRuntime_;
        std::shared_ptr<Locking::LockTable> lockTable_;
        PortManager portManager_;
        bool coldStarted_{false};
    };
} // namespace PickCore

#endif // PICK_SYSTEM_CORE_DAEMON_GEMINI_SERVICE_DAEMON_H
