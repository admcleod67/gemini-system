#ifndef PICK_SYSTEM_CORE_ENGLISH_PARSER_H
#define PICK_SYSTEM_CORE_ENGLISH_PARSER_H

#include "EnglishTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace PickCore::English {
    class EnglishParser {
    public:
        [[nodiscard]] std::optional<Query> parse(const std::vector<std::string> &tokens, std::string &error) const;
    };
} // namespace PickCore::English

#endif
