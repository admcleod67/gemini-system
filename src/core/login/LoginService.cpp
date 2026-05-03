#include "LoginService.h"

#include "Catalog.h"
#include "FileSystem.h"
#include "GeminiCatalog.h"

#include <cctype>
#include <cstdlib>
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

        void trimTrailingAsciiWs(std::string &s) {
            while (!s.empty()) {
                const unsigned char c = static_cast<unsigned char>(s.back());
                if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
                    s.pop_back();
                } else {
                    break;
                }
            }
        }

        std::string firstLineFromRecordBody(const std::string &body) {
            std::string line;
            for (const char ch: body) {
                if (ch == '\n') {
                    break;
                }
                line.push_back(ch);
            }
            trimTrailingAsciiWs(line);
            return line;
        }

        const GeminiAccountRow *findAccountRow(const std::vector<GeminiAccountRow> &accounts, const std::string &accountName) {
            for (const auto &a: accounts) {
                if (iequalsAscii(a.name, accountName)) {
                    return &a;
                }
            }
            return nullptr;
        }

        bool reservedLogonTokenUpper(const std::string &upper) {
            for (const char *name: kReservedLoginNames) {
                if (upper == name) {
                    return true;
                }
            }
            return false;
        }

        bool reservedLogonToken(const std::string &token) {
            std::string upper;
            upper.reserve(token.size());
            for (const unsigned char c: token) {
                upper.push_back(static_cast<char>(std::toupper(c)));
            }
            return reservedLogonTokenUpper(upper);
        }

        std::optional<std::string> readAutoLogonAccountIdFromMd(const std::filesystem::path &pickRoot) {
            try {
                const PickFS::FileSystem fs(pickRoot);
                const std::optional<PickFS::Record> rec = fs.read("MD", "AUTO-LOGON");
                if (!rec.has_value()) {
                    return std::nullopt;
                }
                const std::string accountToken = firstLineFromRecordBody(rec->value());
                if (accountToken.empty()) {
                    return std::nullopt;
                }
                if (!PickFS::Catalog::isValidName(accountToken) || reservedLogonToken(accountToken)) {
                    return std::nullopt;
                }
                return accountToken;
            } catch (const PickFS::FileSystemError &) {
                return std::nullopt;
            }
        }

        std::optional<std::string> readAutoLogonAccountIdFromEnv() {
            const char *acc = std::getenv("GEMINI_AUTO_LOGON");
            if (acc == nullptr || acc[0] == '\0') {
                acc = std::getenv("GEMINI_AUTO_LOGIN");
            }
            if (acc == nullptr || acc[0] == '\0') {
                return std::nullopt;
            }
            std::string tok(acc);
            if (!PickFS::Catalog::isValidName(tok) || reservedLogonToken(tok)) {
                return std::nullopt;
            }
            return tok;
        }

        /// Pick-style: emit `LOGON PLEASE: ` with **no** trailing newline; prompt and account occupy one line.
        void printLogonPleasePrefix(std::ostream &out) {
            out << "LOGON PLEASE: " << std::flush;
        }

        /// Exactly one `\n`, flushed — one blank stdout line before the Tcl banner (no extra leading `\n` in Shell).
        void printlnLoginToTclBoundary(std::ostream &out) {
            out.put('\n');
            out.flush();
        }

        /// Returns **`false`** only if a password line was required and `getline` hit EOF.
        [[nodiscard]] bool readPasswordLineIfAccountRequires(std::istream &in, const GeminiAccountRow *acctRow, std::string &password) {
            password.clear();
            if (acctRow == nullptr || !acctRow->passwordHash.has_value() ||
                passwordPlaceholderSkipsVerify(*acctRow->passwordHash)) {
                return true;
            }
            if (!std::getline(in, password)) {
                return false;
            }
            trimTrailingAsciiWs(password);
            return true;
        }

        /// **`authenticateAccount`** plus successful stdout seal — shared by catalogue auto-login and interactive login.
        std::optional<UserSession>
        authenticateAccountAndSealStdout(std::ostream &out,
                                           const std::filesystem::path &catalogRoot,
                                           const std::string &accountName,
                                           const std::string &password,
                                           std::ostream &err) {
            std::optional<UserSession> sess = LoginService::authenticateAccount(catalogRoot, accountName, password, err);
            if (!sess.has_value()) {
                return std::nullopt;
            }
            printlnLoginToTclBoundary(out);
            return sess;
        }
    } // namespace

    bool LoginService::isReservedLoginUsername(const std::string &token) {
        return reservedLogonToken(token);
    }

    bool LoginService::parseSingleUsernameLine(const std::string &line, std::string &outToken) {
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
        outToken = std::move(tok);
        return true;
    }

    std::optional<UserSession> LoginService::authenticateAccount(const std::filesystem::path &catalogRoot,
                                                                 const std::string &accountName,
                                                                 const std::string &password,
                                                                 std::ostream &err) {
        const std::filesystem::path accountsPath = catalogRoot / "ACCOUNTS.json";
        const auto accounts = GeminiCatalog::loadAccounts(accountsPath);
        if (!accounts.has_value()) {
            err << "Cannot read ACCOUNTS.json.\n";
            return std::nullopt;
        }

        const GeminiAccountRow *acct = nullptr;
        for (const auto &a: *accounts) {
            if (iequalsAscii(a.name, accountName)) {
                acct = &a;
                break;
            }
        }
        if (acct == nullptr) {
            err << "Unknown account.\n";
            return std::nullopt;
        }

        if (acct->passwordHash.has_value() && !passwordPlaceholderSkipsVerify(*acct->passwordHash)) {
            if (password.empty()) {
                err << "Password required.\n";
                return std::nullopt;
            }
            if (password != *acct->passwordHash) {
                err << "Login failed.\n";
                return std::nullopt;
            }
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
        s.username = acct->name;
        s.accountName = acct->name;
        s.whoPort = 0;
        s.userNo = "0";
        return s;
    }

    std::optional<UserSession> LoginService::runCatalogLogin(std::istream &in,
                                                             std::ostream &out,
                                                             const std::filesystem::path &catalogRoot,
                                                             const std::filesystem::path &pickRoot,
                                                             std::ostream *err,
                                                             CatalogLoginPhase phase) {
        if (phase == CatalogLoginPhase::ColdStartPortInit) {
            std::optional<std::string> autoName = readAutoLogonAccountIdFromMd(pickRoot);
            if (!autoName.has_value()) {
                autoName = readAutoLogonAccountIdFromEnv();
            }

            if (autoName.has_value()) {
                printLogonPleasePrefix(out);
                // No keyed Return on stdin — end the echoed `LOGON PLEASE: account` line (like interactive), then optional password / auth.
                out << *autoName << std::endl;

                const auto accountsList = GeminiCatalog::loadAccounts(catalogRoot / "ACCOUNTS.json");
                const GeminiAccountRow *acct =
                    accountsList.has_value() ? findAccountRow(*accountsList, *autoName) : nullptr;
                std::string password;
                if (!readPasswordLineIfAccountRequires(in, acct, password)) {
                    return std::nullopt;
                }

                std::ostringstream errBuf;
                if (const auto sess =
                        authenticateAccountAndSealStdout(out, catalogRoot, *autoName, password, errBuf)) {
                    return sess;
                }
                if (err != nullptr) {
                    *err << errBuf.str();
                }
            }
        }

        return runConsoleLogin(in, out, catalogRoot);
    }

    std::optional<UserSession> LoginService::runConsoleLogin(std::istream &in,
                                                             std::ostream &out,
                                                             const std::filesystem::path &catalogRoot) {
        for (;;) {
            printLogonPleasePrefix(out);
            std::string line;
            if (!std::getline(in, line)) {
                return std::nullopt;
            }
            trimTrailingAsciiWs(line);
            std::string accountName;
            if (!parseSingleUsernameLine(line, accountName)) {
                out << '\n';
                continue;
            }

            const auto accounts = GeminiCatalog::loadAccounts(catalogRoot / "ACCOUNTS.json");
            if (!accounts.has_value()) {
                out << '\n';
                continue;
            }
            const GeminiAccountRow *acct = nullptr;
            for (const auto &a: *accounts) {
                if (iequalsAscii(a.name, accountName)) {
                    acct = &a;
                    break;
                }
            }
            if (acct == nullptr) {
                out << '\n';
                continue;
            }

            std::string password;
            if (!readPasswordLineIfAccountRequires(in, acct, password)) {
                return std::nullopt;
            }

            std::ostringstream err;
            const auto sess = authenticateAccountAndSealStdout(out, catalogRoot, accountName, password, err);
            if (!sess.has_value()) {
                out << '\n';
                continue;
            }
            return sess;
        }
    }
} // namespace PickCore
