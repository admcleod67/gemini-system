//
// Load gemini/USERS.json and gemini/ACCOUNTS.json (host JSON catalogues).
//

#ifndef PICK_SYSTEM_CORE_CATALOG_GEMINICATALOG_H
#define PICK_SYSTEM_CORE_CATALOG_GEMINICATALOG_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace PickCore {
    struct GeminiUserRow {
        std::string username;
        std::string passwordHash;
        std::string defaultAccount;
        std::string privileges;
    };

    struct GeminiAccountRow {
        std::string name;
        /// Path relative to the catalogue directory (parent of ACCOUNTS.json).
        std::string root;
        /// If set: `dev-` prefix skips password prompt; otherwise console reads one password line (no banner).
        std::optional<std::string> passwordHash;
    };

    class GeminiCatalog {
    public:
        [[nodiscard]] static std::optional<std::vector<GeminiUserRow>> loadUsers(const std::filesystem::path &jsonPath);

        [[nodiscard]] static std::optional<std::vector<GeminiAccountRow>> loadAccounts(const std::filesystem::path &jsonPath);
    };
} // namespace PickCore

#endif
