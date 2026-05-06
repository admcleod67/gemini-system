#include "DictionaryResolver.h"

#include <cctype>
#include <optional>

namespace PickCore::English {
    namespace {
        std::optional<int> parsePositiveInt(const std::string &text) {
            if (text.empty()) {
                return std::nullopt;
            }
            for (const char c: text) {
                if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
                    return std::nullopt;
                }
            }
            const int v = std::stoi(text);
            if (v <= 0) {
                return std::nullopt;
            }
            return v;
        }

        std::optional<int> resolveFieldRec(PickFS::FileSystem &fs, const std::string &token, int depth) {
            if (depth > 16) {
                return std::nullopt;
            }
            if (const std::optional<int> numeric = parsePositiveInt(token)) {
                return numeric;
            }
            const std::optional<PickFS::Record> rec = fs.read("DICT", token);
            if (!rec.has_value()) {
                return std::nullopt;
            }
            const auto &attrs = rec->structured();
            const std::string type = attrs.attribute(1).firstValue();
            if (type == "A" || type == "a") {
                return parsePositiveInt(attrs.attribute(2).firstValue());
            }
            if (type == "S" || type == "s") {
                const std::string target = attrs.attribute(2).firstValue();
                if (target.empty()) {
                    return std::nullopt;
                }
                return resolveFieldRec(fs, target, depth + 1);
            }
            return std::nullopt;
        }
    } // namespace

    std::vector<FieldRef> DictionaryResolver::resolveFields(PickFS::FileSystem &fs,
                                                            const std::string & /*fileName*/,
                                                            const std::vector<std::string> &fields) const {
        std::vector<FieldRef> out;
        out.reserve(fields.size());
        for (const std::string &field: fields) {
            out.push_back(FieldRef{field, resolveFieldRec(fs, field, 0)});
        }
        return out;
    }
} // namespace PickCore::English
