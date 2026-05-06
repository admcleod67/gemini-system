#include "StructuredRecord.h"

#include <sstream>

namespace PickFS {
    StructuredRecord::StructuredRecord(std::map<int, RecordAttribute> attributes)
        : attributes_(std::move(attributes)) {
    }

    bool StructuredRecord::hasAttribute(const int attributeNo) const {
        return attributeNo > 0 && attributes_.find(attributeNo) != attributes_.end();
    }

    const RecordAttribute &StructuredRecord::attribute(const int attributeNo) const {
        static const RecordAttribute kEmpty{};
        if (attributeNo <= 0) {
            return kEmpty;
        }
        const auto it = attributes_.find(attributeNo);
        if (it == attributes_.end()) {
            return kEmpty;
        }
        return it->second;
    }

    void StructuredRecord::setAttribute(const int attributeNo, RecordAttribute attribute) {
        if (attributeNo <= 0) {
            return;
        }
        attributes_[attributeNo] = std::move(attribute);
    }

    std::string StructuredRecord::toRaw() const {
        std::string out;
        bool first = true;
        for (const auto &[attributeNo, attr]: attributes_) {
            (void) attributeNo;
            if (!first) {
                out.push_back('\n');
            }
            first = false;
            out += attr.raw();
        }
        return out;
    }

    StructuredRecord StructuredRecord::fromRaw(const std::string &raw) {
        std::map<int, RecordAttribute> attrs;
        std::stringstream in(raw);
        std::string line;
        int attrNo = 1;
        while (std::getline(in, line)) {
            attrs[attrNo++] = RecordAttribute(line);
        }
        if (!raw.empty() && raw.back() == '\n') {
            attrs[attrNo] = RecordAttribute("");
        }
        return StructuredRecord(std::move(attrs));
    }
} // namespace PickFS
