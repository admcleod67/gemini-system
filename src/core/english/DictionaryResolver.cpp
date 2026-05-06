#include "DictionaryResolver.h"

#include "StructuredRecord.h"

#include <algorithm>
#include <cctype>

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

        bool startsWithIc(const std::string &hay, std::string_view needle) {
            if (needle.size() > hay.size()) {
                return false;
            }
            for (std::size_t i = 0; i < needle.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(hay[i])) !=
                    std::tolower(static_cast<unsigned char>(needle[i]))) {
                    return false;
                }
            }
            return true;
        }

        std::string trimmedUpper(const std::string &text) {
            std::size_t start = 0;
            std::size_t end = text.size();
            while (start < end && std::isspace(static_cast<unsigned char>(text[start]))) {
                ++start;
            }
            while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
                --end;
            }
            std::string o = text.substr(start, end - start);
            std::transform(o.begin(),
                           o.end(),
                           o.begin(),
                           [](unsigned char ch) -> char { return static_cast<char>(std::toupper(static_cast<int>(ch))); });
            return o;
        }

        ConversionCode classifyFromAttrs(const PickFS::StructuredRecord &attrs) {
            for (int ai: {10, 9, 8, 7}) {
                const std::string cell = attrs.attribute(ai).firstValue();
                if (cell.empty()) {
                    continue;
                }
                const std::string u = trimmedUpper(cell);
                if (startsWithIc(u, "MD")) {
                    return ConversionCode::MD;
                }
                if (startsWithIc(u, "MC")) {
                    return ConversionCode::MC;
                }
                if (startsWithIc(u, "D")) {
                    return ConversionCode::D;
                }
            }
            return ConversionCode::None;
        }

        std::optional<FieldRef> resolveFieldRec(PickFS::FileSystem &fs, const std::string &token, int depth) {
            if (depth > 16) {
                return std::nullopt;
            }
            if (const std::optional<int> numeric = parsePositiveInt(token)) {
                return FieldRef{token, *numeric, ConversionCode::None};
            }
            const std::optional<PickFS::Record> rec = fs.read("DICT", token);
            if (!rec.has_value()) {
                return std::nullopt;
            }
            const auto &attrs = rec->structured();
            const std::string type = attrs.attribute(1).firstValue();
            if (type == "A" || type == "a") {
                const std::optional<int> attrNum = parsePositiveInt(attrs.attribute(2).firstValue());
                const ConversionCode conv = classifyFromAttrs(attrs);
                if (!attrNum.has_value()) {
                    return FieldRef{token, std::nullopt, conv};
                }
                return FieldRef{token, *attrNum, conv};
            }
            if (type == "S" || type == "s") {
                const std::string target = attrs.attribute(2).firstValue();
                if (target.empty()) {
                    return FieldRef{token, std::nullopt, ConversionCode::None};
                }
                return resolveFieldRec(fs, target, depth + 1);
            }
            return FieldRef{token, std::nullopt, ConversionCode::None};
        }
    } // namespace

    FieldRef DictionaryResolver::resolveField(PickFS::FileSystem &fs, const std::string &token) const {
        if (auto r = resolveFieldRec(fs, token, 0)) {
            return *r;
        }
        return FieldRef{token, std::nullopt, ConversionCode::None};
    }

    std::vector<FieldRef> DictionaryResolver::resolveFields(PickFS::FileSystem &fs,
                                                          const std::string & /*fileName*/,
                                                          const std::vector<std::string> &fields) const {
        std::vector<FieldRef> out;
        out.reserve(fields.size());
        for (const std::string &field: fields) {
            out.push_back(resolveField(fs, field));
        }
        return out;
    }
} // namespace PickCore::English
