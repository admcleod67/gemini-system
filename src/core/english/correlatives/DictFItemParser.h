#ifndef PICK_SYSTEM_CORE_ENGLISH_DICT_F_ITEM_PARSER_H
#define PICK_SYSTEM_CORE_ENGLISH_DICT_F_ITEM_PARSER_H

#include "FCorrelativeDef.h"

#include "StructuredRecord.h"

#include <optional>
#include <string>

namespace PickCore::English {
    class DictFItemParser {
    public:
        /// Parse a DICT record body (`StructuredRecord`) as F-type (attr 1 must be F).
        /// @return Parsed definition or stable error message.
        [[nodiscard]] static std::optional<FCorrelativeDef> parse(const PickFS::StructuredRecord &dictAttrs,
                                                                  std::string &error);
    };
} // namespace PickCore::English

#endif
