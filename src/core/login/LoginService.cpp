#include "LoginService.h"

#include "Catalog.h"
#include "GeminiCatalog.h"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string_view>

namespace PickCore {
    namespace {
        bool iequalsAscii(std::string_view a, std::string_view b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                    return false;
                }
            }
            return true;
        }

        bool passwordPlaceholderSkipsVerify(const std::string &hash) {
            return hash.size() >= 4U && hash.compare(0, 4, "dev-") == 0;
        }

        const char *const kReservedLoginNames[] = {"LOGIN", "QUIT",   "VERSION", "HELP",    "TCL",     "EDIT",
                                                   "RUN",   "BASIC",  "PROC",    "ASM",     "ECHO",    "SET",
                                                   "GET",   "UNSET",  "WHO",     "LOGTO",   "LOGOFF",  "CREATE-FILE",
                                                   "DELETE-FILE", "LIST-FILES", "LIST", "READ", "WRITE", "LIST-PROGRAMS",
                                                   "LIST-VARS", "DUMP-STACK"};
    } // namespace

    bool LoginService::isReservedLoginUsername(const std::string &token) {
        std::string upper;
        upper.reserve(token.size());
        for (const unsigned char c: token) {
            upper.push_back(static_cast<char>(std::toupper(c)));
        }
        for (const char *name: kReservedLoginNames) {
            if (upper == name) {
                return true;
            }
        }
        return false;
    }

    bool LoginService::parseSingleUsernameLine(const std::string &line, std::string &outUsername) {
        std::istringstream iss(line);
        std::string tok;
        if (!(iss >> tok)) {
            return false;
        }
        std::string extra;
        if (iss >> extra) {
            return false;
        }
        if (!PickFS::Catalog::isValidName(tok) || isReservedLoginUsername(tok)) {
            return false;
        }
        outUsername = std::move(tok);
        return true;
    }

    std::optional<UserSession> LoginService::authenticate(const std::filesystem::path &catalogRoot,
                                                          const std::string &username,
                                                          const std::string &password,
                                                          std::ostream &err) {
        const std::filesystem::path usersPath = catalogRoot / "USERS.json";
        const std::filesystem::path accountsPath = catalogRoot / "ACCOUNTS.json";
        const auto users = GeminiCatalog::loadUsers(usersPath);
        const auto accounts = GeminiCatalog::loadAccounts(accountsPath);
        if (!users.has_value() || !accounts.has_value()) {
            err << "Cannot read USERS.json or ACCOUNTS.json.\n";
            return std::nullopt;
        }

        const GeminiUserRow *userRow = nullptr;
        for (const auto &u: *users) {
            if (iequalsAscii(u.username, username)) {
                userRow = &u;
                break;
            }
        }
        if (userRow == nullptr) {
            err << "Unknown user.\n";
            return std::nullopt;
        }

        if (!passwordPlaceholderSkipsVerify(userRow->passwordHash)) {
            if (password.empty()) {
                err << "Password required.\n";
                return std::nullopt;
            }
            if (password != userRow->passwordHash) {
                err << "Login failed.\n";
                return std::nullopt;
            }
        }

        const GeminiAccountRow *acct = nullptr;
        for (const auto &a: *accounts) {
            if (iequalsAscii(a.name, userRow->defaultAccount)) {
                acct = &a;
                break;
            }
        }
        if (acct == nullptr) {
            err << "Default account not found.\n";
            return std::nullopt;
        }

        std::error_code ec;
        const std::filesystem::path pickRoot = (catalogRoot / acct->root).lexically_normal();
        if (!std::filesystem::is_directory(pickRoot / "VOC", ec)) {
            err << "Account path has no VOC: " << pickRoot.string() << '\n';
            return std::nullopt;
        }

        UserSession s;
        s.catalogRoot = catalogRoot;
        s.pickRoot = pickRoot;
        s.username = userRow->username;
        s.accountName = acct->name;
        s.whoPort = 0;
        s.userNo = "0";
        return s;
    }

    std::optional<UserSession> LoginService::tryAutoLoginFromEnv(const std::filesystem::path &catalogRoot,
                                                                 std::ostream *err) {
        const char *const user = std::getenv("GEMINI_AUTO_LOGIN");
        if (user == nullptr || user[0] == '\0') {
            return std::nullopt;
        }
        std::ostringstream errBuf;
        const auto sess = authenticate(catalogRoot, user, "", errBuf);
        if (!sess.has_value() && err != nullptr) {
            *err << errBuf.str();
        }
        return sess;
    }

    std::optional<UserSession> LoginService::runConsoleLogin(std::istream &in,
                                                             std::ostream &out,
                                                             const std::filesystem::path &catalogRoot) {
        out << "LOGON PLEASE:\n";
        out << "Enter your username (EOF to exit).\n";
        for (;;) {
            out << "Username: " << std::flush;
            std::string line;
            if (!std::getline(in, line)) {
                return std::nullopt;
            }
            std::string username;
            if (!parseSingleUsernameLine(line, username)) {
                out << "Invalid username.\n";
                continue;
            }

            const auto users = GeminiCatalog::loadUsers(catalogRoot / "USERS.json");
            if (!users.has_value()) {
                out << "Cannot read USERS.json.\n";
                continue;
            }
            const GeminiUserRow *userRow = nullptr;
            for (const auto &u: *users) {
                if (iequalsAscii(u.username, username)) {
                    userRow = &u;
                    break;
                }
            }
            if (userRow == nullptr) {
                out << "Unknown user.\n";
                continue;
            }

            std::string password;
            if (!passwordPlaceholderSkipsVerify(userRow->passwordHash)) {
                out << "Password: " << std::flush;
                if (!std::getline(in, password)) {
                    return std::nullopt;
                }
            }

            std::ostringstream err;
            const auto sess = authenticate(catalogRoot, username, password, err);
            if (!sess.has_value()) {
                out << err.str();
                continue;
            }
            return sess;
        }
    }
} // namespace PickCore
