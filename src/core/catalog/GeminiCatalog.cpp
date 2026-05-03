#include "GeminiCatalog.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace PickCore {
    std::optional<std::vector<GeminiUserRow>> GeminiCatalog::loadUsers(const std::filesystem::path &jsonPath) {
        try {
            std::ifstream in(jsonPath);
            if (!in) {
                return std::nullopt;
            }
            nlohmann::json j;
            in >> j;
            if (!j.contains("users") || !j["users"].is_array()) {
                return std::nullopt;
            }
            std::vector<GeminiUserRow> out;
            for (const auto &row: j["users"]) {
                GeminiUserRow u;
                u.username = row.value("username", "");
                u.passwordHash = row.value("passwordHash", "");
                u.defaultAccount = row.value("defaultAccount", "");
                u.privileges = row.value("privileges", "");
                out.push_back(std::move(u));
            }
            return out;
        } catch (...) {
            return std::nullopt;
        }
    }

    std::optional<std::vector<GeminiAccountRow>> GeminiCatalog::loadAccounts(const std::filesystem::path &jsonPath) {
        try {
            std::ifstream in(jsonPath);
            if (!in) {
                return std::nullopt;
            }
            nlohmann::json j;
            in >> j;
            if (!j.contains("accounts") || !j["accounts"].is_array()) {
                return std::nullopt;
            }
            std::vector<GeminiAccountRow> out;
            for (const auto &row: j["accounts"]) {
                GeminiAccountRow a;
                a.name = row.value("name", "");
                a.root = row.value("root", "");
                out.push_back(std::move(a));
            }
            return out;
        } catch (...) {
            return std::nullopt;
        }
    }
} // namespace PickCore
