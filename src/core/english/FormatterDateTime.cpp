#include "FormatterDateTime.h"

#include <cstdio>
#include <ctime>

namespace PickCore::English {
    namespace {
        // Day count of the Pick internal epoch (1967-12-31) relative to the Unix epoch.
        // Day 0 = 1967-12-31, so 1970-01-01 = day 732. Kept local to the formatter so the
        // ENGLISH layer does not depend on the VM runtime; a cross-check test in
        // test_EnglishFormatter.cpp catches drift versus the BASIC side.
        constexpr int kPickInternalDateAtUnixEpoch = 732;

        std::tm utcTm(std::time_t t) {
            std::tm tmBuf{};
#if defined(_WIN32)
            gmtime_s(&tmBuf, &t);
#else
            gmtime_r(&t, &tmBuf);
#endif
            return tmBuf;
        }
    } // namespace

    int currentPickDay() {
        const std::time_t now = std::time(nullptr);
        const auto dayUnix = static_cast<int>(now / 86400LL);
        return dayUnix + kPickInternalDateAtUnixEpoch;
    }

    int currentSecondsOfDay() {
        const std::time_t now = std::time(nullptr);
        long long s = static_cast<long long>(now) % 86400LL;
        if (s < 0) {
            s += 86400LL;
        }
        return static_cast<int>(s);
    }

    std::string formatDate(int pickDay) {
        const long long unixDay = static_cast<long long>(pickDay) - kPickInternalDateAtUnixEpoch;
        const std::time_t t = static_cast<std::time_t>(unixDay * 86400LL);
        const std::tm tmBuf = utcTm(t);
        char buf[32]{};
        if (std::strftime(buf, sizeof buf, "%d %b %Y", &tmBuf) == 0) {
            return std::string{};
        }
        return std::string{buf};
    }

    std::string formatTimeOfDay(int seconds, bool wantSeconds) {
        seconds %= 86400;
        if (seconds < 0) {
            seconds += 86400;
        }
        const int h = seconds / 3600;
        const int m = (seconds / 60) % 60;
        const int s = seconds % 60;
        char buf[16]{};
        if (wantSeconds) {
            std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", h, m, s);
        } else {
            std::snprintf(buf, sizeof buf, "%02d:%02d", h, m);
        }
        return std::string{buf};
    }
} // namespace PickCore::English
