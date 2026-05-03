//
// Core login: catalogue authentication and console LOGON phase (account-based).
//

#ifndef PICK_SYSTEM_CORE_LOGIN_LOGINSERVICE_H
#define PICK_SYSTEM_CORE_LOGIN_LOGINSERVICE_H

#include "UserSession.h"

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

namespace PickCore {
    class LoginService {
    public:
        /// Authenticate by account name in ACCOUNTS.json; optional per-account `passwordHash` (see GeminiAccountRow).
        [[nodiscard]] static std::optional<UserSession> authenticateAccount(const std::filesystem::path &catalogRoot,
                                                                            const std::string &accountName,
                                                                            const std::string &password,
                                                                            std::ostream &err);

        /// Cold-start catalogue login after `BootMonitor`: always prints `LOGON PLEASE:` — or
        /// `LOGON PLEASE: <account>` when MD/env auto-account applies — before creating a session
        /// and returning. Interactive entry is used if auto-account is absent or fails.
        [[nodiscard]] static std::optional<UserSession> runCatalogLogin(std::istream &in,
                                                                        std::ostream &out,
                                                                        const std::filesystem::path &catalogRoot,
                                                                        const std::filesystem::path &pickRoot,
                                                                        std::ostream *err = nullptr);

        /// Interactive console login. Prints only `LOGON PLEASE:` per line; returns `std::nullopt` on EOF.
        [[nodiscard]] static std::optional<UserSession> runConsoleLogin(std::istream &in,
                                                                        std::ostream &out,
                                                                        const std::filesystem::path &catalogRoot);

        static bool isReservedLoginUsername(const std::string &token);

        /// One non-reserved Pick name token (used for account id at logon).
        static bool parseSingleUsernameLine(const std::string &line, std::string &outToken);
    };
} // namespace PickCore

#endif
