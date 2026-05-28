#include "DictIItemParser.h"

#include "IExpressionParser.h"

#include <cctype>
#include <string>

namespace PickCore::English {
    namespace {
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
    } // namespace

    std::optional<ICorrelativeDef> DictIItemParser::parse(const PickFS::StructuredRecord &dictAttrs,
                                                          std::string &error) {
        error.clear();
        const std::string type = dictAttrs.attribute(1).firstValue();
        if (type != "I" && type != "i") {
            error = "Invalid I-type DICT item: not an I-type record";
            return std::nullopt;
        }

        const std::string expression = trimmed(dictAttrs.attribute(2).firstValue());
        if (expression.empty()) {
            error = "Invalid I-type DICT item: missing expression";
            return std::nullopt;
        }

        std::string parseError;
        std::unique_ptr<IExpr> ast = IExpressionParser::parse(expression, parseError);
        if (!ast) {
            error = parseError.empty() ? std::string{"I-type: invalid expression"} : parseError;
            return std::nullopt;
        }

        ICorrelativeDef def;
        def.expressionRaw = expression;
        def.expression = std::shared_ptr<const IExpr>(std::move(ast));
        return def;
    }
} // namespace PickCore::English
