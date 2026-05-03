//
// Core login: catalogue authentication and console LOGON phase.
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
        /// Build a session from username/password; writes errors to `err` on failure.
        [[nodiscard]] static std::optional<UserSession> authenticate(const std::filesystem::path &catalogRoot,
                                                                     const std::string &username,
                                                                     const std::string &password,
                                                                     std::ostream &err);

        /// If `GEMINI_AUTO_LOGIN` is set, attempt authenticate for that username (empty password).
        /// On failure, if `err` is non-null, writes the same messages as `authenticate`.
        [[nodiscard]] static std::optional<UserSession> tryAutoLoginFromEnv(const std::filesystem::path &catalogRoot,
                                                                            std::ostream *err = nullptr);

        /// Interactive console login. Returns `std::nullopt` on EOF (exit host). On success returns session.
        [[nodiscard]] static std::optional<UserSession> runConsoleLogin(std::istream &in,
                                                                        std::ostream &out,
                                                                        const std::filesystem::path &catalogRoot);

        static bool isReservedLoginUsername(const std::string &token);

        static bool parseSingleUsernameLine(const std::string &line, std::string &outUsername);
    };
} // namespace PickCore

#endif
