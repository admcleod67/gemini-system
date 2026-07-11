#include "BootMonitor.h"

#include "GeminiCatalog.h"
#include "LanguageModuleLoader.h"
#include "LanguageRegistry.h"
#include "PortManager.h"

#include <pick_system/version.hpp>
#include <Runtime.h>

#include <filesystem>

namespace PickCore {
    void BootMonitor::runColdStart(std::ostream &out, const BootContext &ctx) {
        out << pick_system::system_title << ' ' << pick_system::version_string << '\n';
        out << "INITIALIZING SYSTEM...\n";
        out << "MEMORY: (host n/a)\n";

        out << "ATTACHING SYSTEM FILES...\n";
        if (ctx.hostPaths.pickFilesystemRoot.has_value()) {
            std::error_code ec;
            const std::filesystem::path md = *ctx.hostPaths.pickFilesystemRoot / "MD";
            const std::filesystem::path voc = *ctx.hostPaths.pickFilesystemRoot / "VOC";
            if (std::filesystem::is_directory(md, ec) && !ec) {
                out << "MD ATTACHED\n";
            } else {
                out << "MD: (not present)\n";
            }
            if (std::filesystem::is_directory(voc, ec) && !ec) {
                out << "VOC ATTACHED\n";
            } else {
                out << "VOC: (not present)\n";
            }
        } else {
            out << "MD: (skip — no Pick root)\n";
            out << "VOC: (skip — no Pick root)\n";
        }

        if (ctx.hostPaths.geminiCatalogRoot.has_value()) {
            const std::filesystem::path cat = *ctx.hostPaths.geminiCatalogRoot;
            const auto users = GeminiCatalog::loadUsers(cat / "USERS.json");
            if (users.has_value()) {
                out << "USERS ATTACHED\n";
            } else {
                const std::filesystem::path accountsPath = cat / "ACCOUNTS.json";
                if (!std::filesystem::exists(accountsPath)) {
                    out << "CATALOG: (missing ACCOUNTS.json)\n";
                } else {
                    const auto accounts = GeminiCatalog::loadAccounts(accountsPath);
                    if (accounts.has_value()) {
                        bool invalidRows = false;
                        for (const auto &row: *accounts) {
                            if (row.name.empty() || row.root.empty()) {
                                invalidRows = true;
                                break;
                            }
                        }
                        if (invalidRows) {
                            out << "CATALOG: (invalid account rows)\n";
                        } else {
                            out << "CATALOG ATTACHED\n";
                        }
                    } else {
                        out << "CATALOG: (malformed ACCOUNTS.json)\n";
                    }
                }
            }
        } else {
            out << "CATALOG: (skip — no catalogue root)\n";
        }

        if (ctx.hostPaths.geminiCatalogRoot.has_value()) {
            const std::filesystem::path cat = *ctx.hostPaths.geminiCatalogRoot;
            const std::filesystem::path accountsPath = cat / "ACCOUNTS.json";
            const auto accounts = GeminiCatalog::loadAccounts(accountsPath);
            if (accounts.has_value()) {
                for (const auto &row: *accounts) {
                    if (row.name.empty() || row.root.empty()) {
                        continue;
                    }
                    std::error_code ec;
                    const std::filesystem::path accountRoot = (cat / row.root).lexically_normal();
                    if (!std::filesystem::is_directory(accountRoot, ec)) {
                        out << "ACCOUNT " << row.name << ": (root not present)\n";
                        continue;
                    }
                    if (!std::filesystem::is_directory(accountRoot / "VOC", ec)) {
                        out << "ACCOUNT " << row.name << ": (VOC not present)\n";
                    }
                    if (!std::filesystem::is_directory(accountRoot / "MD", ec)) {
                        out << "ACCOUNT " << row.name << ": (MD not present)\n";
                    }
                }
            }
        }

        if (ctx.hostPaths.geminiModulesRoot.has_value()) {
            Languages::loadLanguageModules(
                Languages::LanguageRegistry::instance(), *ctx.hostPaths.geminiModulesRoot, &out, ctx.runtime);
        } else {
            out << "MODULES: (skip — no modules path)\n";
            Languages::LanguageRegistry::instance().freeze();
        }

        if (ctx.portManager != nullptr) {
            ctx.portManager->formatBootStatus(out);
        } else {
            out << "PORT MANAGER: (stub)\n";
        }
        out << "SYSTEM READY\n\n";
        out.flush();
    }
} // namespace PickCore
