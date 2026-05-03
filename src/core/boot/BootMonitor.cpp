#include "BootMonitor.h"

#include "GeminiCatalog.h"

#include <pick_system/version.hpp>
#include <Runtime.h>

#include <filesystem>

namespace PickCore {
    void BootMonitor::runColdStart(std::ostream &out, const BootContext &ctx) {
        (void) ctx.runtime;
        out << pick_system::system_title << ' ' << pick_system::version_string << '\n';
        out << "INITIALIZING SYSTEM...\n";
        out << "MEMORY: (host n/a)\n";

        out << "ATTACHING SYSTEM FILES...\n";
        if (ctx.hostPaths.pickFilesystemRoot.has_value()) {
            std::error_code ec;
            const std::filesystem::path md = *ctx.hostPaths.pickFilesystemRoot / "MD";
            if (std::filesystem::is_directory(md, ec) && !ec) {
                out << "MD ATTACHED\n";
            } else {
                out << "MD: (not present)\n";
            }
        } else {
            out << "MD: (skip — no Pick root)\n";
        }

        if (ctx.hostPaths.geminiCatalogRoot.has_value()) {
            const std::filesystem::path cat = *ctx.hostPaths.geminiCatalogRoot;
            const auto users = GeminiCatalog::loadUsers(cat / "USERS.json");
            if (users.has_value()) {
                out << "USERS ATTACHED\n";
            } else {
                const auto accounts = GeminiCatalog::loadAccounts(cat / "ACCOUNTS.json");
                if (accounts.has_value()) {
                    out << "CATALOG ATTACHED\n";
                } else {
                    out << "CATALOG: (not loaded)\n";
                }
            }
        } else {
            out << "CATALOG: (skip — no catalogue root)\n";
        }

        out << "PORT MANAGER: (stub)\n";
        out << "SYSTEM READY\n";
    }
} // namespace PickCore
