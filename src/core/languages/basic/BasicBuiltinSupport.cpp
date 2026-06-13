#include "BasicBuiltinSupport.h"

#include "PickOconv.h"
#include "PickIconv.h"

#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

namespace PickCore::Languages::Basic {
    namespace {
        std::string upperCopy(const std::string &s) {
            std::string r;
            r.reserve(s.size());
            for (const char c : s) {
                r += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return r;
        }
    } // namespace

    std::string valueToString(const PickVM::Value &v) {
        if (std::holds_alternative<std::string>(v)) {
            return std::get<std::string>(v);
        }
        if (std::holds_alternative<double>(v)) {
            std::ostringstream oss;
            oss << std::setprecision(15) << std::get<double>(v);
            std::string out = oss.str();
            if (out.find('.') != std::string::npos) {
                while (!out.empty() && out.back() == '0') {
                    out.pop_back();
                }
                if (!out.empty() && out.back() == '.') {
                    out.push_back('0');
                }
            }
            return out;
        }
        return std::to_string(std::get<int>(v));
    }

    double coerceToDouble(const PickVM::Value &v) {
        if (std::holds_alternative<int>(v)) {
            return static_cast<double>(std::get<int>(v));
        }
        if (std::holds_alternative<double>(v)) {
            return std::get<double>(v);
        }
        const std::string &s = std::get<std::string>(v);
        if (s.empty()) {
            return 0.0;
        }
        char *endp = nullptr;
        errno = 0;
        const double n = std::strtod(s.c_str(), &endp);
        if (endp == s.c_str() || errno != 0) {
            return 0.0;
        }
        return n;
    }

    int coerceToInt(const PickVM::Value &v) {
        const double n = coerceToDouble(v);
        if (n < static_cast<double>(std::numeric_limits<int>::min()) ||
            n > static_cast<double>(std::numeric_limits<int>::max())) {
            return 0;
        }
        return static_cast<int>(n);
    }

    bool isFullyNumeric(const std::string &s) {
        if (s.empty()) {
            return false;
        }
        char *endp = nullptr;
        errno = 0;
        (void) std::strtod(s.c_str(), &endp);
        if (endp == s.c_str() || errno != 0) {
            return false;
        }
        while (*endp != '\0' && std::isspace(static_cast<unsigned char>(*endp))) {
            ++endp;
        }
        return *endp == '\0';
    }

    int daysFromCivil(const int y, const int m, const int d) {
        const int year = y - (m <= 2 ? 1 : 0);
        const int era = (year >= 0 ? year : year - 399) / 400;
        const unsigned yoe = static_cast<unsigned>(year - era * 400);
        const unsigned doy =
            (153u * static_cast<unsigned>(m > 2 ? m - 3 : m + 9) + 2u) / 5u + static_cast<unsigned>(d) - 1u;
        const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
        return era * 146097 + static_cast<int>(doe) - 719468;
    }

    std::string dateStringFromPickDay(const int day) {
        const long long unixDay = static_cast<long long>(day) - kPickInternalDateAtUnixEpoch;
        const std::time_t t = static_cast<std::time_t>(unixDay * 86400LL);
        std::tm tmBuf{};
#if defined(_WIN32)
        gmtime_s(&tmBuf, &t);
#else
        gmtime_r(&t, &tmBuf);
#endif
        char buf[32]{};
        if (std::strftime(buf, sizeof buf, "%d %b %Y", &tmBuf) == 0) {
            return std::string{};
        }
        return std::string{buf};
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
        for (const char c : monStr) {
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

    std::string timeStringFromSeconds(int n, const bool wantSeconds) {
        n %= 86400;
        if (n < 0) {
            n += 86400;
        }
        const int h = n / 3600;
        const int m = (n / 60) % 60;
        const int s = n % 60;
        char buf[16]{};
        if (wantSeconds) {
            std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", h, m, s);
        } else {
            std::snprintf(buf, sizeof buf, "%02d:%02d", h, m);
        }
        return std::string{buf};
    }

    int secondsFromTimeString(const std::string &s, const bool wantSeconds, bool &ok) {
        ok = false;
        int h = 0, m = 0, sec = 0;
        char c1 = 0, c2 = 0;
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

    std::string formatMdScaled(const long long intVal, const int decimals) {
        if (decimals == 0) {
            return std::to_string(intVal);
        }
        const bool negative = intVal < 0;
        unsigned long long absVal =
            negative ? static_cast<unsigned long long>(-(intVal + 1)) + 1ULL
                     : static_cast<unsigned long long>(intVal);
        unsigned long long divisor = 1;
        for (int i = 0; i < decimals; ++i) {
            divisor *= 10ULL;
        }
        const unsigned long long intPart = absVal / divisor;
        const unsigned long long fracPart = absVal % divisor;
        char buf[64]{};
        std::snprintf(buf, sizeof buf, "%s%llu.%0*llu",
                      negative ? "-" : "",
                      intPart,
                      decimals,
                      fracPart);
        return std::string{buf};
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

    PickVM::Value dispatchConvertCode(const ConvertDirection dir,
                                      const PickVM::Value &value,
                                      const std::string &codeRaw) {
        if (dir == ConvertDirection::Output) {
            return PickCore::Conversion::oconvOutputBuiltin(valueToString(value), codeRaw);
        }

        const std::string code = upperCopy(codeRaw);
        const std::string input = valueToString(value);
        const std::string converted = PickCore::Conversion::iconvOutputBuiltin(input, codeRaw);
        if (code == "MCU" || code == "MCL") {
            return converted;
        }
        return coerceToInt(converted);
    }
} // namespace PickCore::Languages::Basic
