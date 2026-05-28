#include "PickOconv.h"

#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <ctime>
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

        int coerceStringToInt(const std::string &s) {
            if (s.empty()) {
                return 0;
            }
            char *endp = nullptr;
            errno = 0;
            const double n = std::strtod(s.c_str(), &endp);
            if (endp == s.c_str() || errno != 0) {
                return 0;
            }
            if (n < static_cast<double>(std::numeric_limits<int>::min()) ||
                n > static_cast<double>(std::numeric_limits<int>::max())) {
                return 0;
            }
            return static_cast<int>(n);
        }

        std::string dateStringFromPickDay(int day) {
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

        std::string timeStringFromSeconds(int n, bool wantSeconds) {
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

        std::string formatMdScaled(long long intVal, int decimals) {
            if (decimals == 0) {
                return std::to_string(intVal);
            }
            const bool negative = intVal < 0;
            unsigned long long absVal = negative ? static_cast<unsigned long long>(-(intVal + 1)) + 1ULL
                                                   : static_cast<unsigned long long>(intVal);
            unsigned long long divisor = 1;
            for (int i = 0; i < decimals; ++i) {
                divisor *= 10ULL;
            }
            const unsigned long long intPart = absVal / divisor;
            const unsigned long long fracPart = absVal % divisor;
            char buf[64]{};
            std::snprintf(buf,
                          sizeof buf,
                          "%s%llu.%0*llu",
                          negative ? "-" : "",
                          intPart,
                          decimals,
                          fracPart);
            return std::string{buf};
        }

        std::optional<std::string> oconvOutputImpl(const std::string &value,
                                                   const std::string &code,
                                                   const bool builtinErrors) {
            const int asInt = coerceStringToInt(value);

            if (code == "D") {
                return dateStringFromPickDay(asInt);
            }
            if (code == "MT" || code == "MTS") {
                return timeStringFromSeconds(asInt, code == "MTS");
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
                return formatMdScaled(static_cast<long long>(asInt), decimals);
            }

            if (builtinErrors) {
                throw std::runtime_error("BUILTIN: OCONV unsupported code \"" + code + "\"");
            }
            return std::nullopt;
        }
    } // namespace

    std::optional<std::string> oconvOutput(const std::string &value,
                                           const std::string &codeRaw,
                                           std::string &error) {
        error.clear();
        const std::string trimmedCode = trimmed(codeRaw);
        const std::string code = upperCopy(trimmedCode);
        if (const std::optional<std::string> out = oconvOutputImpl(value, code, false)) {
            return out;
        }
        error = "F-type: unsupported conversion code \"" + trimmedCode + "\"";
        return std::nullopt;
    }

    std::string oconvOutputBuiltin(const std::string &value, const std::string &codeRaw) {
        const std::string code = upperCopy(trimmed(codeRaw));
        if (const std::optional<std::string> out = oconvOutputImpl(value, code, true)) {
            return *out;
        }
        throw std::runtime_error("BUILTIN: OCONV unsupported code \"" + codeRaw + "\"");
    }
} // namespace PickCore::Conversion
