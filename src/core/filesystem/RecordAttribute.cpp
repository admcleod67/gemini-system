#include "RecordAttribute.h"

namespace PickFS {
    namespace {
        constexpr char kValueMark = static_cast<char>(0xFD);
    }

    std::string joinValues(const std::vector<std::string> &values) {
        std::string out;
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                out.push_back(kValueMark);
            }
            out += values[i];
        }
        return out;
    }

    std::vector<std::string> RecordAttribute::splitValues() const {
        std::vector<std::string> out;
        std::string current;
        for (const char c: raw_) {
            if (c == kValueMark) {
                out.push_back(current);
                current.clear();
                continue;
            }
            current.push_back(c);
        }
        out.push_back(current);
        return out;
    }

    std::string RecordAttribute::valueAt(const int valueNo) const {
        if (valueNo <= 0) {
            return "";
        }
        const std::vector<std::string> values = splitValues();
        const std::size_t idx = static_cast<std::size_t>(valueNo - 1);
        if (idx >= values.size()) {
            return "";
        }
        return values[idx];
    }
} // namespace PickFS
