#ifndef PICK_SYSTEM_CORE_ENGLISH_TYPES_H
#define PICK_SYSTEM_CORE_ENGLISH_TYPES_H

#include <optional>
#include <string>
#include <vector>

namespace PickCore::English {
    enum class Verb {
        LIST,
        COUNT,
        SELECT,
        SORT,
    };

    enum class ConversionCode {
        None,
        D,
        MD,
        MC,
    };

    struct SortKeySpec {
        std::string fieldToken;
        bool descending{false};
    };

    struct Query {
        Verb verb;
        /// Logical Pick file containing records (from token or imposed by caller).
        std::string fileName;
        std::vector<std::string> fields;
        /// Only used when verb == SORT; empty means sort by item-id only.
        std::vector<SortKeySpec> sortKeys;
        /// Raw HEADING template (Milestone 8 Stage 1). std::nullopt when no HEADING clause.
        /// `@`-token substitution is performed by the formatter at render time.
        std::optional<std::string> heading;
    };

    struct Plan {
        Query query;
    };

    struct FieldRef {
        std::string token;
        std::optional<int> attributeNo;
        ConversionCode conversion{ConversionCode::None};
    };

    /// Internal handoff between Executor (row production) and Formatter (rendering) — Milestone 8.
    /// Each Row represents one projected record before any layout/heading work.
    struct Row {
        std::string id;
        std::vector<std::string> projectedFields;
    };

    struct Result {
        std::vector<std::string> lines;
        std::vector<std::string> selectedIds;
    };

    struct EnglishRunOptions {
        /// When set, only these record ids are scanned (active-list scope).
        std::optional<std::vector<std::string>> constrainRecordIds;
        /// Lines per page override for HEADING reports (Milestone 8 Stage 2). When
        /// std::nullopt the formatter uses its default (24 lines). Set by the Tcl
        /// shell from the session-level `SET PAGE-LENGTH n` value.
        std::optional<int> pageLength;
    };
} // namespace PickCore::English

#endif
