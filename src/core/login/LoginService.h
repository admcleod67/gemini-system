//
// Core login: catalogue authentication and console LOGON phase (account-based).
//

#ifndef PICK_SYSTEM_CORE_LOGIN_LOGINSERVICE_H
#define PICK_SYSTEM_CORE_LOGIN_LOGINSERVICE_H

#include "GeminiCatalog.h"
#include "UserSession.h"

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

namespace PickCore {
    /// `ColdStartPortInit`: first catalogue logon after boot — may use `MD,AUTO-LOGON` and `GEMINI_AUTO_LOGON`.
    /// `InteractiveOnly`: e.g. after `LOGOFF` — plain `LOGON PLEASE:` only (no auto-login).
    enum class CatalogLoginPhase {
        ColdStartPortInit,
        InteractiveOnly,
    };

    class LoginService {
    public:
        /// Authenticate by account name in ACCOUNTS.json; optional per-account `passwordHash` (see GeminiAccountRow).
        [[nodiscard]] static std::optional<UserSession> authenticateAccount(const std::filesystem::path &catalogRoot,
                                                                            const std::string &accountName,
                                                                            const std::string &password,
                                                                            std::ostream &err);

        /// If `acctRow` requires a password (non-optional hash, not `dev-` placeholder), read one line from `in`
        /// into `password`. Returns `false` only when a line was required and `getline` hit EOF before a line.
        [[nodiscard]] static bool readPasswordLineIfAccountRequires(std::istream &in,
                                                                    const GeminiAccountRow *acctRow,
                                                                    std::string &password);

        /// After `BootMonitor`. For `ColdStartPortInit` only, may read `MD,AUTO-LOGON` / env and print
        /// `LOGON PLEASE: <account>` then **`endl`** (no stdin Enter — terminates the echoed line like interactive).
        /// Password read if required, then auth; on success the **same** `println` boundary as interactive (one
        /// flushed `\n` before TCL). **`InteractiveOnly`** prints `LOGON PLEASE: ` with no newline until
        /// **`getline`** consumes the typed account line. Tcl prints its banner with **no leading newline**.
        [[nodiscard]] static std::optional<UserSession> runCatalogLogin(std::istream &in,
                                                                        std::ostream &out,
                                                                        const std::filesystem::path &catalogRoot,
                                                                        const std::filesystem::path &pickRoot,
                                                                        std::ostream *err,
                                                                        CatalogLoginPhase phase);

        /// Interactive console login. Prints `LOGON PLEASE: ` with no newline (`print`-style flush); reads the
        /// account line; on success emits one `\n` flushed (`println` boundary). Returns `std::nullopt` on EOF.
        [[nodiscard]] static std::optional<UserSession> runConsoleLogin(std::istream &in,
                                                                        std::ostream &out,
                                                                        const std::filesystem::path &catalogRoot);

        static bool isReservedLoginUsername(const std::string &token);

        /// One non-reserved Pick name token (used for account id at logon).
        static bool parseSingleUsernameLine(const std::string &line, std::string &outToken);
    };
} // namespace PickCore

#endif
