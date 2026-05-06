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
    };

    struct Plan {
        Query query;
    };

    struct FieldRef {
        std::string token;
        std::optional<int> attributeNo;
        ConversionCode conversion{ConversionCode::None};
    };

    struct Result {
        std::vector<std::string> lines;
        std::vector<std::string> selectedIds;
    };

    struct EnglishRunOptions {
        /// When set, only these record ids are scanned (active-list scope).
        std::optional<std::vector<std::string>> constrainRecordIds;
    };
} // namespace PickCore::English

#endif
