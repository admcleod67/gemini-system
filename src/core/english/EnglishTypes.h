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
    };

    struct Query {
        Verb verb;
        std::string fileName;
        std::vector<std::string> fields;
    };

    struct Plan {
        Query query;
    };

    struct FieldRef {
        std::string token;
        std::optional<int> attributeNo;
    };

    struct Result {
        std::vector<std::string> lines;
        std::vector<std::string> selectedIds;
    };
} // namespace PickCore::English

#endif
