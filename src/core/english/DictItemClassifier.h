#ifndef PICK_SYSTEM_CORE_ENGLISH_DICT_ITEM_CLASSIFIER_H
#define PICK_SYSTEM_CORE_ENGLISH_DICT_ITEM_CLASSIFIER_H

#include "StructuredRecord.h"

#include <string>

namespace PickCore::English {
    struct DictItemSummary {
        std::string typeLabel; // A | S | F | I | INVALID
        bool valid = false;
        std::string error;
    };

    class DictItemClassifier {
    public:
        /// Structural validity of a DICT record body (no data-file or S-chain resolution).
        [[nodiscard]] static DictItemSummary classify(const PickFS::StructuredRecord &attrs);
    };
} // namespace PickCore::English

#endif
