#ifndef PICK_SYSTEM_CORE_ENGLISH_TYPES_H
#define PICK_SYSTEM_CORE_ENGLISH_TYPES_H

#include "correlatives/FCorrelativeDef.h"
#include "correlatives/ICorrelativeDef.h"

#include <optional>
#include <string>
#include <vector>

namespace PickCore::English {
    enum class DictFieldKind {
        Attribute,
        FCorrelative,
        ICorrelative,
    };
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
        /// Field token from `BREAK-ON <field>` (Milestone 8 Stage 3). Resolved in the executor.
        std::optional<std::string> breakOnField;
        /// Field token from `TOTAL <field>` (Milestone 8 Stage 4). Resolved in the executor.
        std::optional<std::string> totalField;
        /// When true, `ID-SUPP` suppresses record ids on formatted data rows (M8 Stage 5).
        bool idSupp{false};
        /// Raw FOOTING template (M8 Stage 6). std::nullopt when no FOOTING clause.
        std::optional<std::string> footing;
    };

    struct Plan {
        Query query;
    };

    struct FieldRef {
        std::string token;
        DictFieldKind kind{DictFieldKind::Attribute};
        std::optional<int> attributeNo;
        ConversionCode conversion{ConversionCode::None};
        /// Populated when `kind == DictFieldKind::FCorrelative` (Milestone 9 Stage 1).
        std::optional<FCorrelativeDef> fCorrelative;
        /// Populated when `kind == DictFieldKind::ICorrelative` (Milestone 9 Stage 4).
        std::optional<ICorrelativeDef> iCorrelative;
    };

    [[nodiscard]] inline bool fieldRefIsResolved(const FieldRef &ref) {
        if (ref.kind == DictFieldKind::FCorrelative) {
            return ref.fCorrelative.has_value();
        }
        if (ref.kind == DictFieldKind::ICorrelative) {
            return ref.iCorrelative.has_value() && ref.iCorrelative->expression != nullptr;
        }
        return ref.attributeNo.has_value();
    }

    /// Internal handoff between Executor (row production) and Formatter (rendering) — Milestone 8.
    /// Each Row represents one projected record before any layout/heading work.
    struct Row {
        std::string id;
        std::vector<std::string> projectedFields;
        /// Resolved break-field cell (first sub-value). Empty when absent or no attribute.
        /// Only meaningful when `Query::breakOnField` is set.
        std::string breakKey;
        /// Numeric addend for TOTAL when `Query::totalField` is set. 0 when cell is empty/non-numeric.
        double totalAddend{0};
        /// 1-based attribute first-values for HEADING `@n` substitution (index 0 = attribute 1).
        std::vector<std::string> headingAttrs;
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
