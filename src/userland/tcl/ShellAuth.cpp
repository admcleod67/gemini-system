#include "Shell.h"

#include "Catalog.h"
#include "GeminiCatalog.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace PickShell {
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

    void Shell::setGeminiCatalogRoot(std::optional<std::filesystem::path> root) {
        session_.setGeminiCatalogRoot(std::move(root));
    }

    void Shell::tryAutoLoginFromEnvironment(std::ostream &warn) {
        if (!session_.geminiCatalogRoot().has_value()) {
            return;
        }
        const char *const user = std::getenv("GEMINI_AUTO_LOGIN");
        if (user == nullptr || user[0] == '\0') {
            return;
        }
        std::ostringstream err;
        if (!performLogin(user, "", err)) {
            warn << err.str();
        }
    }

    bool Shell::needsGeminiLogin() const {
        return session_.geminiCatalogRoot().has_value() && !session_.loggedIn();
    }

    bool Shell::isReservedLoginUsername(const std::string &token) {
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

    bool Shell::parseSingleUsernameLine(const std::string &line, std::string &outUsername) {
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

    bool Shell::performLogin(const std::string &username, const std::string &password, std::ostream &err) {
        const auto &cat = session_.geminiCatalogRoot();
        if (!cat.has_value()) {
            err << "No Gemini catalogue configured.\n";
            return false;
        }
        const std::filesystem::path usersPath = *cat / "USERS.json";
        const std::filesystem::path accountsPath = *cat / "ACCOUNTS.json";
        const auto users = GeminiCatalog::loadUsers(usersPath);
        const auto accounts = GeminiCatalog::loadAccounts(accountsPath);
        if (!users.has_value() || !accounts.has_value()) {
            err << "Cannot read USERS.json or ACCOUNTS.json.\n";
            return false;
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
            return false;
        }

        if (!passwordPlaceholderSkipsVerify(userRow->passwordHash)) {
            if (password.empty()) {
                err << "Password required.\n";
                return false;
            }
            if (password != userRow->passwordHash) {
                err << "Login failed.\n";
                return false;
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
            return false;
        }

        std::error_code ec;
        const std::filesystem::path pickRoot = (*cat / acct->root).lexically_normal();
        if (!std::filesystem::is_directory(pickRoot / "VOC", ec)) {
            err << "Account path has no VOC: " << pickRoot.string() << '\n';
            return false;
        }

        session_.setFileSystemRoot(pickRoot);
        session_.resetForQuit();
        session_.setSessionIdentity(0, userRow->username, acct->name);
        session_.setLoggedIn(true);
        (void) session_.env_.set("@USERNO", "0");
        (void) session_.env_.set("@ACCOUNT", acct->name);
        (void) session_.env_.set("@LOGNAME", userRow->username);
        return true;
    }

    bool Shell::runLoginPhase(std::istream &in, std::ostream &out) {
        out << "Gemini login\n";
        out << "Enter your username (EOF to exit).\n";
        for (;;) {
            out << "Username: " << std::flush;
            std::string line;
            if (!std::getline(in, line)) {
                return false;
            }
            std::string username;
            if (!parseSingleUsernameLine(line, username)) {
                out << "Invalid username.\n";
                continue;
            }

            std::string password;
            const auto users = GeminiCatalog::loadUsers(*session_.geminiCatalogRoot() / "USERS.json");
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

            if (!passwordPlaceholderSkipsVerify(userRow->passwordHash)) {
                out << "Password: " << std::flush;
                if (!std::getline(in, password)) {
                    return false;
                }
            }

            std::ostringstream err;
            if (!performLogin(username, password, err)) {
                out << err.str();
                continue;
            }
            return true;
        }
    }

    void Shell::runInteractiveLoop() {
        std::istream &in = inputStream_ != nullptr ? *inputStream_ : std::cin;
        std::ostream &out = std::cout;

        for (;;) {
            if (needsGeminiLogin()) {
                if (!runLoginPhase(in, out)) {
                    return;
                }
            }

            out << "Gemini/TCL Developer Shell\n";
            out << "Type HELP for commands\n";

            bool quit = false;
            while (!quit) {
                if (needsGeminiLogin()) {
                    break;
                }
                out << prompt() << std::flush;
                std::string line;
                if (!std::getline(in, line)) {
                    return;
                }
                handleLine(line, out, quit);
                if (returnToLoginAfterLogoff_) {
                    returnToLoginAfterLogoff_ = false;
                    break;
                }
            }
            if (quit) {
                return;
            }
        }
    }

    void Shell::cmdLogto(const std::vector<std::string> &tokens, std::ostream &out) {
        if (!session_.loggedIn()) {
            out << "Not logged in.\n";
            return;
        }
        if (tokens.size() != 2U) {
            out << "LOGTO requires an account name\n";
            return;
        }
        const auto &cat = session_.geminiCatalogRoot();
        if (!cat.has_value()) {
            out << "No Gemini catalogue configured.\n";
            return;
        }
        const auto accounts = GeminiCatalog::loadAccounts(*cat / "ACCOUNTS.json");
        if (!accounts.has_value()) {
            out << "Cannot read ACCOUNTS.json.\n";
            return;
        }
        const GeminiAccountRow *acct = nullptr;
        for (const auto &a: *accounts) {
            if (iequalsAscii(a.name, tokens[1])) {
                acct = &a;
                break;
            }
        }
        if (acct == nullptr) {
            out << "Unknown account.\n";
            return;
        }
        std::error_code ec;
        const std::filesystem::path pickRoot = (*cat / acct->root).lexically_normal();
        if (!std::filesystem::is_directory(pickRoot / "VOC", ec)) {
            out << "Account path has no VOC.\n";
            return;
        }
        session_.setFileSystemRoot(pickRoot);
        session_.resetForQuit();
        session_.setSessionIdentity(session_.whoPort(), session_.sessionUsername(), acct->name);
        (void) session_.env_.set("@USERNO", "0");
        (void) session_.env_.set("@ACCOUNT", acct->name);
        (void) session_.env_.set("@LOGNAME", session_.sessionUsername());
    }

    void Shell::cmdLogoff(const std::vector<std::string> &tokens, std::ostream &out) {
        (void) tokens;
        if (!session_.loggedIn()) {
            out << "Not logged in.\n";
            return;
        }
        session_.clearLoginSession();
        session_.resetForQuit();
        returnToLoginAfterLogoff_ = true;
        out << "Logged off.\n";
    }

} // namespace PickShell
