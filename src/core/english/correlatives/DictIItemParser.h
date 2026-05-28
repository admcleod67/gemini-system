#ifndef PICK_SYSTEM_CORE_ENGLISH_DICT_I_ITEM_PARSER_H
#define PICK_SYSTEM_CORE_ENGLISH_DICT_I_ITEM_PARSER_H

#include "ICorrelativeDef.h"

#include "StructuredRecord.h"

#include <optional>
#include <string>

namespace PickCore::English {
    class DictIItemParser {
    public:
        [[nodiscard]] static std::optional<ICorrelativeDef> parse(const PickFS::StructuredRecord &dictAttrs,
                                                                  std::string &error);
    };
} // namespace PickCore::English

#endif
