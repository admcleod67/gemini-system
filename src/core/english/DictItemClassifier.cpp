#include "DictItemClassifier.h"

#include "correlatives/DictFItemParser.h"
#include "correlatives/DictIItemParser.h"

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

        DictItemSummary invalidSummary(const std::string &error = std::string{}) {
            DictItemSummary summary;
            summary.typeLabel = "INVALID";
            summary.valid = false;
            summary.error = error;
            return summary;
        }
    } // namespace

    DictItemSummary DictItemClassifier::classify(const PickFS::StructuredRecord &attrs) {
        const std::string type = trimmed(attrs.attribute(1).firstValue());
        if (type.empty()) {
            return invalidSummary("Invalid DICT item: missing type code");
        }

        if (type == "A" || type == "a") {
            DictItemSummary summary;
            summary.typeLabel = "A";
            if (parsePositiveInt(trimmed(attrs.attribute(2).firstValue())).has_value()) {
                summary.valid = true;
            } else {
                summary.error = "Invalid A-type DICT item: bad attribute number";
            }
            return summary;
        }

        if (type == "S" || type == "s") {
            DictItemSummary summary;
            summary.typeLabel = "S";
            if (!trimmed(attrs.attribute(2).firstValue()).empty()) {
                summary.valid = true;
            } else {
                summary.error = "Invalid S-type DICT item: missing synonym target";
            }
            return summary;
        }

        if (type == "F" || type == "f") {
            DictItemSummary summary;
            summary.typeLabel = "F";
            std::string parseError;
            if (DictFItemParser::parse(attrs, parseError).has_value()) {
                summary.valid = true;
            } else {
                summary.error = parseError;
            }
            return summary;
        }

        if (type == "I" || type == "i") {
            DictItemSummary summary;
            summary.typeLabel = "I";
            std::string parseError;
            if (DictIItemParser::parse(attrs, parseError).has_value()) {
                summary.valid = true;
            } else {
                summary.error = parseError;
            }
            return summary;
        }

        return invalidSummary("Invalid DICT item: unknown type code \"" + type + "\"");
    }
} // namespace PickCore::English
