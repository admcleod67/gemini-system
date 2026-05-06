//
// Attribute-indexed view over a Pick record body.
//

#ifndef PICK_SYSTEM_FILESYSTEM_STRUCTUREDRECORD_H
#define PICK_SYSTEM_FILESYSTEM_STRUCTUREDRECORD_H

#include "RecordAttribute.h"

#include <map>
#include <string>

namespace PickFS {
    class StructuredRecord {
    public:
        StructuredRecord() = default;
        explicit StructuredRecord(std::map<int, RecordAttribute> attributes);

        [[nodiscard]] const std::map<int, RecordAttribute> &attributes() const { return attributes_; }

        [[nodiscard]] bool hasAttribute(int attributeNo) const;
        [[nodiscard]] const RecordAttribute &attribute(int attributeNo) const;
        void setAttribute(int attributeNo, RecordAttribute attribute);

        [[nodiscard]] std::string toRaw() const;
        static StructuredRecord fromRaw(const std::string &raw);

    private:
        std::map<int, RecordAttribute> attributes_;
    };
} // namespace PickFS

#endif // PICK_SYSTEM_FILESYSTEM_STRUCTUREDRECORD_H
