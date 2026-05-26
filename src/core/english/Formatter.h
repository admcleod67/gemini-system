#ifndef PICK_SYSTEM_CORE_ENGLISH_FORMATTER_H
#define PICK_SYSTEM_CORE_ENGLISH_FORMATTER_H

#include "EnglishTypes.h"

#include <string>
#include <vector>

namespace PickCore::English {
    /// Inputs used by the formatter that would otherwise be read from the wall clock.
    /// Tests inject deterministic values; production calls `defaultFormatterContext()`.
    struct FormatterContext {
        /// 1-based page number at the start of rendering. Incremented by the formatter
        /// at each page boundary; `@PAGE` substitution reads the live counter.
        int pageNumber{1};
        /// Lines per page for `HEADING` queries (Milestone 8 Stage 2). Values <= 0 disable
        /// pagination entirely (useful as an escape hatch and for legacy callers).
        int pageLength{24};
        /// Pick internal day count for `@DATE` substitution.
        int currentPickDay{0};
        /// Seconds since UTC midnight for `@TIME` substitution.
        int currentSecondsOfDay{0};
    };

    /// Build a context using today's date/time and pageNumber=1.
    [[nodiscard]] FormatterContext defaultFormatterContext();

    /// Take the executor's row stream and lay it out into the final Result.
    ///
    /// Output shape: `[heading?, row*, trailing*]` where `heading` is rendered from
    /// `plan.query.heading` with `@`-token substitution, rows are formatted as
    /// `"<id>[ <field1>...]"`, and `trailingLines` are summary lines verbatim
    /// (e.g. "5 records selected" for SELECT, the count for COUNT).
    [[nodiscard]] Result format(const Plan &plan,
                                std::vector<Row> rows,
                                std::vector<std::string> trailingLines,
                                std::vector<std::string> selectedIds,
                                const FormatterContext &ctx);
} // namespace PickCore::English

#endif
