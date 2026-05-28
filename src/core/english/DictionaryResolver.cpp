#include "DictionaryResolver.h"

#include "correlatives/DictFItemParser.h"
#include "correlatives/DictIItemParser.h"

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

        std::optional<PickFS::Record> tryReadDictRecord(PickFS::FileSystem &fs,
                                                        const std::string &dictLogicalFile,
                                                        const std::string &token) {
            try {
                return fs.read(dictLogicalFile, token);
            } catch (const PickFS::FileSystemError &) {
                return std::nullopt;
            }
        }

        bool logicalFileExists(PickFS::FileSystem &fs, const std::string &logicalName) {
            try {
                (void) fs.openFile(logicalName);
                return true;
            } catch (const PickFS::FileSystemError &) {
                return false;
            }
        }

        FieldRef makeAttributeRef(const std::string &token,
                                  const std::optional<int> &attrNum,
                                  const ConversionCode conv) {
            FieldRef ref;
            ref.token = token;
            ref.kind = DictFieldKind::Attribute;
            ref.attributeNo = attrNum;
            ref.conversion = conv;
            return ref;
        }

        FieldRef makeUnresolvedRef(const std::string &token) {
            return makeAttributeRef(token, std::nullopt, ConversionCode::None);
        }

        std::optional<FieldRef> fieldFromStructuredDict(const PickFS::StructuredRecord &attrs,
                                                        const std::string &originalToken) {
            const std::string type = attrs.attribute(1).firstValue();
            if (type == "A" || type == "a") {
                const std::optional<int> attrNum = parsePositiveInt(attrs.attribute(2).firstValue());
                const ConversionCode conv = classifyFromAttrs(attrs);
                return makeAttributeRef(originalToken, attrNum, conv);
            }
            if (type == "F" || type == "f") {
                std::string parseError;
                if (const std::optional<FCorrelativeDef> def = DictFItemParser::parse(attrs, parseError)) {
                    FieldRef ref;
                    ref.token = originalToken;
                    ref.kind = DictFieldKind::FCorrelative;
                    ref.fCorrelative = *def;
                    return ref;
                }
                return std::nullopt;
            }
            if (type == "I" || type == "i") {
                std::string parseError;
                if (const std::optional<ICorrelativeDef> def = DictIItemParser::parse(attrs, parseError)) {
                    FieldRef ref;
                    ref.token = originalToken;
                    ref.kind = DictFieldKind::ICorrelative;
                    ref.iCorrelative = *def;
                    return ref;
                }
                return std::nullopt;
            }
            if (type == "S" || type == "s") {
                return std::nullopt;
            }
            return makeUnresolvedRef(originalToken);
        }

        std::optional<FieldRef> resolveFieldRec(PickFS::FileSystem &fs,
                                                const std::string &originalToken,
                                                const std::string &lookupToken,
                                                const int depth,
                                                const std::string &dataFileName) {
            if (depth > 16) {
                return std::nullopt;
            }
            if (const std::optional<int> numeric = parsePositiveInt(lookupToken)) {
                return makeAttributeRef(originalToken, numeric, ConversionCode::None);
            }

            const std::optional<PickFS::Record> scoped = [&]() -> std::optional<PickFS::Record> {
                if (dataFileName.empty()) {
                    return std::nullopt;
                }
                const std::string scopedName = DictionaryResolver::scopedDictLogicalName(dataFileName);
                if (!logicalFileExists(fs, scopedName)) {
                    return std::nullopt;
                }
                return tryReadDictRecord(fs, scopedName, lookupToken);
            }();

            const std::optional<PickFS::Record> global = tryReadDictRecord(fs, "DICT", lookupToken);

            const PickFS::Record *chosen = nullptr;
            if (scoped.has_value()) {
                chosen = &*scoped;
            } else if (global.has_value()) {
                chosen = &*global;
            }
            if (chosen == nullptr) {
                return std::nullopt;
            }

            const auto &structured = chosen->structured();
            const std::string type = structured.attribute(1).firstValue();
            if (type == "S" || type == "s") {
                const std::string target = structured.attribute(2).firstValue();
                if (target.empty()) {
                    return makeUnresolvedRef(originalToken);
                }
                return resolveFieldRec(fs, originalToken, target, depth + 1, dataFileName);
            }
            if (auto f = fieldFromStructuredDict(structured, originalToken)) {
                return *f;
            }
            return makeUnresolvedRef(originalToken);
        }
    } // namespace

    std::string DictionaryResolver::scopedDictLogicalName(const std::string &dataFileName) {
        return std::string{"DICT-"} + dataFileName;
    }

    std::string DictionaryResolver::describeConversion(const FieldRef &ref) {
        switch (ref.conversion) {
            case ConversionCode::D:
                return "D";
            case ConversionCode::MD:
                return "MD";
            case ConversionCode::MC:
                return "MC";
            case ConversionCode::None:
            default:
                return "none";
        }
    }

    std::string DictionaryResolver::describeFieldKind(const FieldRef &ref) {
        if (ref.kind == DictFieldKind::FCorrelative) {
            return "F";
        }
        if (ref.kind == DictFieldKind::ICorrelative) {
            return "I";
        }
        if (ref.attributeNo.has_value()) {
            return "A";
        }
        return "unknown";
    }

    std::string DictionaryResolver::describeIExpression(const ICorrelativeDef &def) {
        return def.expressionRaw;
    }

    std::string DictionaryResolver::describeFSelector(const FCorrelativeDef &def) {
        switch (def.selectorKind) {
            case FSelectorKind::ValueIndex:
                return "value " + std::to_string(def.valueIndex);
            case FSelectorKind::Leftmost:
                return "leftmost (L)";
            case FSelectorKind::Rightmost:
                return "rightmost (R)";
            case FSelectorKind::ConversionRaw:
                return "conversion OCONV \"" + def.tailRaw + "\"";
        }
        return "conversion OCONV \"" + def.tailRaw + "\"";
    }

    FieldRef DictionaryResolver::resolveField(PickFS::FileSystem &fs,
                                              const std::string &dataFileName,
                                              const std::string &token) const {
        if (auto r = resolveFieldRec(fs, token, token, 0, dataFileName)) {
            return *r;
        }
        return makeUnresolvedRef(token);
    }

    std::vector<FieldRef> DictionaryResolver::resolveFields(PickFS::FileSystem &fs,
                                                            const std::string &fileName,
                                                            const std::vector<std::string> &fields) const {
        std::vector<FieldRef> out;
        out.reserve(fields.size());
        for (const std::string &field: fields) {
            out.push_back(resolveField(fs, fileName, field));
        }
        return out;
    }
} // namespace PickCore::English
