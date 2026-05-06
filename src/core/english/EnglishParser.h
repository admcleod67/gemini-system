#ifndef PICK_SYSTEM_CORE_ENGLISH_PARSER_H
#define PICK_SYSTEM_CORE_ENGLISH_PARSER_H

#include "EnglishTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace PickCore::English {
    struct ParseContext {
        /// Caller supplies filename (active-list scope) because it was omitted on the Tcl line.
        bool implicitFile{false};
        /// Mandatory when implicitFile — logical file backing the constrained ids / reads.
        std::optional<std::string> imposedFileName;
    };

    class EnglishParser {
    public:
        [[nodiscard]] std::optional<Query> parse(const std::vector<std::string> &tokens,
                                                 const ParseContext &ctx,
                                                 std::string &error) const;
    };
} // namespace PickCore::English

#endif
