#ifndef PICK_SYSTEM_CORE_ENGLISH_FCORRELATIVE_DEF_H
#define PICK_SYSTEM_CORE_ENGLISH_FCORRELATIVE_DEF_H

#include <string>

namespace PickCore::English {
    /// Classified selector from DICT attribute 3 on an F-type item.
    enum class FSelectorKind {
        ValueIndex,
        Leftmost,
        Rightmost,
        ConversionRaw,
    };

    /// Parsed F-type DICT definition (attrs 1–3). Milestone 9 Stage 1.
    struct FCorrelativeDef {
        int sourceAttributeNo{};
        std::string tailRaw;
        FSelectorKind selectorKind{FSelectorKind::ConversionRaw};
        /// 1-based value index when `selectorKind == ValueIndex`.
        int valueIndex{0};
    };
} // namespace PickCore::English

#endif
