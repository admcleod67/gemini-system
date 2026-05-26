#ifndef PICK_SYSTEM_CORE_ENGLISH_FORMATTER_DATETIME_H
#define PICK_SYSTEM_CORE_ENGLISH_FORMATTER_DATETIME_H

#include <string>

namespace PickCore::English {
    /// Pick "internal" day count for the current wall-clock day (UTC).
    /// Day 732 corresponds to 1970-01-01, matching the BASIC `DATE` builtin.
    [[nodiscard]] int currentPickDay();

    /// Seconds since UTC midnight (0..86399), matching the BASIC `TIME` builtin.
    [[nodiscard]] int currentSecondsOfDay();

    /// Render a Pick day-count as `dd MMM yyyy` (e.g. day 732 -> `01 Jan 1970`).
    /// Byte-identical to OCONV "D" on the BASIC side; a unit test cross-checks day 732.
    [[nodiscard]] std::string formatDate(int pickDay);

    /// Render seconds-since-midnight as `HH:MM:SS` (wantSeconds=true) or `HH:MM`.
    [[nodiscard]] std::string formatTimeOfDay(int seconds, bool wantSeconds);
} // namespace PickCore::English

#endif
