//
// Attribute payload wrapper for Pick-style records.
//

#ifndef PICK_SYSTEM_FILESYSTEM_RECORDATTRIBUTE_H
#define PICK_SYSTEM_FILESYSTEM_RECORDATTRIBUTE_H

#include <string>
#include <vector>

namespace PickFS {
    class RecordAttribute {
    public:
        RecordAttribute() = default;
        explicit RecordAttribute(std::string raw) : raw_(std::move(raw)) {
        }

        [[nodiscard]] const std::string &raw() const { return raw_; }

        /// Pick multi-value split using value-mark (0xFD).
        [[nodiscard]] std::vector<std::string> splitValues() const;

        /// 1-indexed value accessor. Missing index => empty string.
        [[nodiscard]] std::string valueAt(const int valueNo) const;

        [[nodiscard]] std::string firstValue() const { return valueAt(1); }

        [[nodiscard]] bool empty() const { return raw_.empty(); }

    private:
        std::string raw_;
    };
} // namespace PickFS

#endif // PICK_SYSTEM_FILESYSTEM_RECORDATTRIBUTE_H
