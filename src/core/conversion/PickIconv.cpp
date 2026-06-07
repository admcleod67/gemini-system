#include "PickIconv.h"

#include <cctype>
#include <climits>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace PickCore::Conversion {
    namespace {
        constexpr int kPickInternalDateAtUnixEpoch = 732;

        std::string trimmed(const std::string &text) {
            std::size_t start = 0;
            std::size_t end = text.size();
            while (start < end && std::isspace(static_cast<unsigned char>(text[start]))) {
                ++start;
            }
            while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
                --end;
            }
            return text.substr(start, end - start);
        }

        std::string upperCopy(const std::string &s) {
            std::string r;
            r.reserve(s.size());
            for (const char c: s) {
                r += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return r;
        }

        int daysFromCivil(const int y, const int m, const int d) {
            const int yAdj = y - (m <= 2 ? 1 : 0);
            const int era = (yAdj >= 0 ? yAdj : yAdj - 399) / 400;
            const unsigned yoe = static_cast<unsigned>(yAdj - era * 400);
            const unsigned doy =
                (153u * static_cast<unsigned>(m > 2 ? m - 3 : m + 9) + 2u) / 5u + static_cast<unsigned>(d) - 1u;
            const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
            return era * 146097 + static_cast<int>(doe) - 719468;
        }

        int pickDayFromDateString(const std::string &s, bool &ok) {
            ok = false;
            std::istringstream iss(s);
            int day = 0;
            std::string monStr;
            int year = 0;
            if (!(iss >> day >> monStr >> year)) {
                return 0;
            }
            std::string extra;
            if (iss >> extra) {
                return 0;
            }
            if (day < 1 || day > 31 || year < 1 || year > 9999) {
                return 0;
            }
            static const char *months[12] = {"jan", "feb", "mar", "apr", "may", "jun",
                                             "jul", "aug", "sep", "oct", "nov", "dec"};
            std::string monLower;
            monLower.reserve(monStr.size());
            for (const char c: monStr) {
                monLower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            int monthIdx = -1;
            if (monLower.size() >= 3) {
                const std::string head = monLower.substr(0, 3);
                for (int i = 0; i < 12; ++i) {
                    if (head == months[i]) {
                        monthIdx = i;
                        break;
                    }
                }
            }
            if (monthIdx < 0) {
                return 0;
            }
            ok = true;
            return daysFromCivil(year, monthIdx + 1, day) + kPickInternalDateAtUnixEpoch;
        }

        int secondsFromTimeString(const std::string &s, const bool wantSeconds, bool &ok) {
            ok = false;
            int h = 0;
            int m = 0;
            int sec = 0;
            char c1 = 0;
            char c2 = 0;
            std::istringstream iss(s);
            if (wantSeconds) {
                if (!(iss >> h >> c1 >> m >> c2 >> sec)) {
                    return 0;
                }
                if (c1 != ':' || c2 != ':') {
                    return 0;
                }
            } else {
                if (!(iss >> h >> c1 >> m)) {
                    return 0;
                }
                if (c1 != ':') {
                    return 0;
                }
            }
            std::string extra;
            if (iss >> extra) {
                return 0;
            }
            if (h < 0 || h > 23 || m < 0 || m > 59 || sec < 0 || sec > 59) {
                return 0;
            }
            ok = true;
            return h * 3600 + m * 60 + sec;
        }

        long long parseMdScaled(const std::string &s, const int decimals, bool &ok) {
            ok = false;
            if (s.empty()) {
                return 0;
            }
            std::size_t i = 0;
            bool negative = false;
            if (s[i] == '+' || s[i] == '-') {
                negative = (s[i] == '-');
                ++i;
            }
            long long intPart = 0;
            bool sawDigit = false;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
                intPart = intPart * 10 + (s[i] - '0');
                sawDigit = true;
                ++i;
            }
            long long fracPart = 0;
            int fracDigits = 0;
            int roundDigit = -1;
            if (i < s.size() && s[i] == '.') {
                ++i;
                while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
                    const int d = s[i] - '0';
                    if (fracDigits < decimals) {
                        fracPart = fracPart * 10 + d;
                        ++fracDigits;
                    } else if (roundDigit < 0) {
                        roundDigit = d;
                    }
                    sawDigit = true;
                    ++i;
                }
            }
            if (i != s.size() || !sawDigit) {
                return 0;
            }
            while (fracDigits < decimals) {
                fracPart *= 10;
                ++fracDigits;
            }
            long long divisor = 1;
            for (int k = 0; k < decimals; ++k) {
                divisor *= 10;
            }
            long long val = intPart * divisor + fracPart;
            if (roundDigit >= 5) {
                val += 1;
            }
            if (negative) {
                val = -val;
            }
            ok = true;
            return val;
        }

        std::optional<std::string> iconvOutputImpl(const std::string &value,
                                                   const std::string &code,
                                                   const bool builtinErrors) {
            if (code == "D") {
                bool ok = false;
                const int day = pickDayFromDateString(value, ok);
                if (!ok) {
                    if (builtinErrors) {
                        throw std::runtime_error("BUILTIN: ICONV invalid \"D\" input");
                    }
                    return std::nullopt;
                }
                return std::to_string(day);
            }
            if (code == "MT" || code == "MTS") {
                const bool wantSeconds = (code == "MTS");
                bool ok = false;
                const int n = secondsFromTimeString(value, wantSeconds, ok);
                if (!ok) {
                    if (builtinErrors) {
                        throw std::runtime_error(std::string("BUILTIN: ICONV invalid \"") + code + "\" input");
                    }
                    return std::nullopt;
                }
                return std::to_string(n);
            }
            if (code == "MCU" || code == "MCL") {
                const bool toUpper = (code == "MCU");
                std::string s = value;
                for (char &c: s) {
                    c = static_cast<char>(toUpper ? std::toupper(static_cast<unsigned char>(c))
                                                  : std::tolower(static_cast<unsigned char>(c)));
                }
                return s;
            }
            if (code == "MD" || code == "MD2") {
                const int decimals = (code == "MD2") ? 2 : 0;
                bool ok = false;
                const long long n = parseMdScaled(value, decimals, ok);
                if (!ok || n < std::numeric_limits<int>::min() || n > std::numeric_limits<int>::max()) {
                    if (builtinErrors) {
                        throw std::runtime_error(std::string("BUILTIN: ICONV invalid \"") + code + "\" input");
                    }
                    return std::nullopt;
                }
                return std::to_string(static_cast<int>(n));
            }

            if (builtinErrors) {
                throw std::runtime_error("BUILTIN: ICONV unsupported code \"" + code + "\"");
            }
            return std::nullopt;
        }
    } // namespace

    std::optional<std::string> iconvOutput(const std::string &value,
                                           const std::string &codeRaw,
                                           std::string &error) {
        error.clear();
        const std::string trimmedCode = trimmed(codeRaw);
        const std::string code = upperCopy(trimmedCode);
        if (const std::optional<std::string> out = iconvOutputImpl(value, code, false)) {
            return out;
        }
        if (code == "D" || code == "MT" || code == "MTS" || code == "MD" || code == "MD2") {
            error = "I-type: invalid ICONV \"" + trimmedCode + "\" input";
            return std::nullopt;
        }
        error = "I-type: unsupported conversion code \"" + trimmedCode + "\"";
        return std::nullopt;
    }

    std::string iconvOutputBuiltin(const std::string &value, const std::string &codeRaw) {
        const std::string code = upperCopy(trimmed(codeRaw));
        if (const std::optional<std::string> out = iconvOutputImpl(value, code, true)) {
            return *out;
        }
        throw std::runtime_error("BUILTIN: ICONV unsupported code \"" + codeRaw + "\"");
    }
} // namespace PickCore::Conversion
