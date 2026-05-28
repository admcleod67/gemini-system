#include "DictFItemParser.h"

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

        std::string trimmed(const std::string &text) {
            std::size_t start = 0;
            std::size_t end = text.size();
            while (start < end && std::isspace(static_cast<unsigned char>(text[start]))) {
                ++start;
            }
            while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
                --end;
            }
            return text.substr(start, end - start);
        }

        std::string upperAscii(const std::string &text) {
            std::string o = text;
            for (char &c: o) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return o;
        }
    } // namespace

    std::optional<FCorrelativeDef> DictFItemParser::parse(const PickFS::StructuredRecord &dictAttrs,
                                                          std::string &error) {
        error.clear();
        const std::string type = dictAttrs.attribute(1).firstValue();
        if (type != "F" && type != "f") {
            error = "Invalid F-type DICT item: not an F-type record";
            return std::nullopt;
        }

        const std::string sourceText = trimmed(dictAttrs.attribute(2).firstValue());
        if (sourceText.empty()) {
            error = "Invalid F-type DICT item: missing source attribute";
            return std::nullopt;
        }
        const std::optional<int> sourceAttr = parsePositiveInt(sourceText);
        if (!sourceAttr.has_value()) {
            error = "Invalid F-type DICT item: bad source attribute number";
            return std::nullopt;
        }

        const std::string tail = trimmed(dictAttrs.attribute(3).firstValue());
        if (tail.empty()) {
            error = "Invalid F-type DICT item: missing selector";
            return std::nullopt;
        }

        FCorrelativeDef def;
        def.sourceAttributeNo = *sourceAttr;
        def.tailRaw = tail;

        const std::string tailUpper = upperAscii(tail);
        if (tailUpper == "L") {
            def.selectorKind = FSelectorKind::Leftmost;
            return def;
        }
        if (tailUpper == "R") {
            def.selectorKind = FSelectorKind::Rightmost;
            return def;
        }
        if (const std::optional<int> valueNo = parsePositiveInt(tail)) {
            def.selectorKind = FSelectorKind::ValueIndex;
            def.valueIndex = *valueNo;
            return def;
        }

        def.selectorKind = FSelectorKind::ConversionRaw;
        return def;
    }
} // namespace PickCore::English
