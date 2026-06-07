#include "IExpressionEvaluator.h"

#include "../../conversion/PickIconv.h"
#include "../../conversion/PickOconv.h"

#include <cmath>
#include <cctype>
#include <sstream>
#include <string>

namespace PickCore::English {
    namespace {
        enum class ValueKind {
            Numeric,
            Text,
        };

        struct EvalValue {
            ValueKind kind{ValueKind::Numeric};
            double number{0};
            std::string text;
        };

        void trimInPlace(std::string &s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
                s.erase(s.begin());
            }
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
                s.pop_back();
            }
        }

        std::optional<double> tryCoerceNumeric(const std::string &raw) {
            std::string s = raw;
            trimInPlace(s);
            if (s.empty()) {
                return 0.0;
            }
            for (char &c: s) {
                if (c == ',' || c == '$') {
                    c = ' ';
                }
            }
            trimInPlace(s);
            if (s.empty()) {
                return 0.0;
            }
            try {
                std::size_t pos = 0;
                const double v = std::stod(s, &pos);
                if (pos == 0) {
                    return std::nullopt;
                }
                return v;
            } catch (const std::exception &) {
                return std::nullopt;
            }
        }

        std::string formatNumericResult(const double value) {
            if (std::isfinite(value) && std::trunc(value) == value) {
                const long long asInt = static_cast<long long>(value);
                return std::to_string(asInt);
            }
            std::ostringstream out;
            out.precision(15);
            out << value;
            std::string s = out.str();
            if (const std::size_t dot = s.find('.'); dot != std::string::npos) {
                while (!s.empty() && s.back() == '0') {
                    s.pop_back();
                }
                if (!s.empty() && s.back() == '.') {
                    s.pop_back();
                }
            }
            return s;
        }

        std::string fieldCellText(const PickFS::StructuredRecord &dataRecord, const int attributeNo) {
            if (!dataRecord.hasAttribute(attributeNo)) {
                return {};
            }
            return dataRecord.attribute(attributeNo).firstValue();
        }

        std::optional<EvalValue> evaluateValue(const IExpr &expr,
                                               const PickFS::StructuredRecord &dataRecord,
                                               std::string &error);

        std::optional<double> evaluateNumeric(const IExpr &expr,
                                              const PickFS::StructuredRecord &dataRecord,
                                              std::string &error) {
            const std::optional<EvalValue> value = evaluateValue(expr, dataRecord, error);
            if (!value.has_value()) {
                return std::nullopt;
            }
            if (value->kind == ValueKind::Text) {
                if (const std::optional<double> n = tryCoerceNumeric(value->text)) {
                    return *n;
                }
                error = "I-type: type mismatch";
                return std::nullopt;
            }
            return value->number;
        }

        bool compareValues(const EvalValue &left,
                           const EvalValue &right,
                           const ICompareOp op,
                           std::string &error) {
            if (left.kind == ValueKind::Numeric && right.kind == ValueKind::Numeric) {
                const double l = left.number;
                const double r = right.number;
                switch (op) {
                    case ICompareOp::Eq:
                        return l == r;
                    case ICompareOp::Ne:
                        return l != r;
                    case ICompareOp::Lt:
                        return l < r;
                    case ICompareOp::Le:
                        return l <= r;
                    case ICompareOp::Gt:
                        return l > r;
                    case ICompareOp::Ge:
                        return l >= r;
                }
            }

            const std::string lText = left.kind == ValueKind::Text ? left.text : formatNumericResult(left.number);
            const std::string rText = right.kind == ValueKind::Text ? right.text : formatNumericResult(right.number);
            if (const std::optional<double> ln = tryCoerceNumeric(lText)) {
                if (const std::optional<double> rn = tryCoerceNumeric(rText)) {
                    switch (op) {
                        case ICompareOp::Eq:
                            return *ln == *rn;
                        case ICompareOp::Ne:
                            return *ln != *rn;
                        case ICompareOp::Lt:
                            return *ln < *rn;
                        case ICompareOp::Le:
                            return *ln <= *rn;
                        case ICompareOp::Gt:
                            return *ln > *rn;
                        case ICompareOp::Ge:
                            return *ln >= *rn;
                    }
                }
            }

            const int cmp = lText.compare(rText);
            switch (op) {
                case ICompareOp::Eq:
                    return cmp == 0;
                case ICompareOp::Ne:
                    return cmp != 0;
                case ICompareOp::Lt:
                    return cmp < 0;
                case ICompareOp::Le:
                    return cmp <= 0;
                case ICompareOp::Gt:
                    return cmp > 0;
                case ICompareOp::Ge:
                    return cmp >= 0;
            }
            error = "I-type: invalid expression";
            return false;
        }

        bool evaluateCondition(const IExpr &expr,
                               const PickFS::StructuredRecord &dataRecord,
                               std::string &error) {
            if (const auto *compare = std::get_if<IExprCompare>(&expr.payload)) {
                if (!compare->left || !compare->right) {
                    error = "I-type: invalid expression";
                    return false;
                }
                const std::optional<EvalValue> left = evaluateValue(*compare->left, dataRecord, error);
                if (!left.has_value()) {
                    return false;
                }
                const std::optional<EvalValue> right = evaluateValue(*compare->right, dataRecord, error);
                if (!right.has_value()) {
                    return false;
                }
                return compareValues(*left, *right, compare->op, error);
            }

            const std::optional<double> numeric = evaluateNumeric(expr, dataRecord, error);
            if (!numeric.has_value()) {
                return false;
            }
            return *numeric != 0.0;
        }

        std::optional<std::string> evaluateCodeArg(const IExpr &expr,
                                                   const PickFS::StructuredRecord &dataRecord,
                                                   std::string &error) {
            const std::optional<EvalValue> value = evaluateValue(expr, dataRecord, error);
            if (!value.has_value()) {
                return std::nullopt;
            }
            if (value->kind == ValueKind::Text) {
                return value->text;
            }
            return formatNumericResult(value->number);
        }

        std::optional<EvalValue> evaluateValue(const IExpr &expr,
                                               const PickFS::StructuredRecord &dataRecord,
                                               std::string &error) {
            if (const auto *number = std::get_if<IExprNumber>(&expr.payload)) {
                EvalValue out;
                out.kind = ValueKind::Numeric;
                out.number = number->value;
                return out;
            }
            if (const auto *str = std::get_if<IExprString>(&expr.payload)) {
                EvalValue out;
                out.kind = ValueKind::Text;
                out.text = str->value;
                return out;
            }
            if (const auto *field = std::get_if<IExprField>(&expr.payload)) {
                EvalValue out;
                out.kind = ValueKind::Text;
                out.text = fieldCellText(dataRecord, field->attributeNo);
                return out;
            }
            if (const auto *unary = std::get_if<IExprUnary>(&expr.payload)) {
                if (!unary->operand) {
                    error = "I-type: invalid expression";
                    return std::nullopt;
                }
                const std::optional<double> operand = evaluateNumeric(*unary->operand, dataRecord, error);
                if (!operand.has_value()) {
                    return std::nullopt;
                }
                EvalValue out;
                out.kind = ValueKind::Numeric;
                out.number = unary->op == '-' ? -(*operand) : *operand;
                return out;
            }
            if (const auto *binary = std::get_if<IExprBinary>(&expr.payload)) {
                if (!binary->left || !binary->right) {
                    error = "I-type: invalid expression";
                    return std::nullopt;
                }
                const std::optional<double> left = evaluateNumeric(*binary->left, dataRecord, error);
                if (!left.has_value()) {
                    return std::nullopt;
                }
                const std::optional<double> right = evaluateNumeric(*binary->right, dataRecord, error);
                if (!right.has_value()) {
                    return std::nullopt;
                }
                EvalValue out;
                out.kind = ValueKind::Numeric;
                switch (binary->op) {
                    case '+':
                        out.number = *left + *right;
                        return out;
                    case '-':
                        out.number = *left - *right;
                        return out;
                    case '*':
                        out.number = *left * *right;
                        return out;
                    case '/':
                        if (*right == 0.0) {
                            error = "I-type: division by zero";
                            return std::nullopt;
                        }
                        out.number = *left / *right;
                        return out;
                    default:
                        error = "I-type: invalid expression";
                        return std::nullopt;
                }
            }
            if (const auto *conditional = std::get_if<IExprIf>(&expr.payload)) {
                if (!conditional->condition || !conditional->thenExpr || !conditional->elseExpr) {
                    error = "I-type: invalid expression";
                    return std::nullopt;
                }
                const bool truth = evaluateCondition(*conditional->condition, dataRecord, error);
                if (!error.empty()) {
                    return std::nullopt;
                }
                return evaluateValue(truth ? *conditional->thenExpr : *conditional->elseExpr, dataRecord, error);
            }
            if (const auto *call = std::get_if<IExprCall>(&expr.payload)) {
                if (!call->valueArg || !call->codeArg) {
                    error = "I-type: invalid expression";
                    return std::nullopt;
                }
                const std::optional<EvalValue> valueArg = evaluateValue(*call->valueArg, dataRecord, error);
                if (!valueArg.has_value()) {
                    return std::nullopt;
                }
                const std::string input =
                    valueArg->kind == ValueKind::Text ? valueArg->text : formatNumericResult(valueArg->number);
                const std::optional<std::string> code = evaluateCodeArg(*call->codeArg, dataRecord, error);
                if (!code.has_value()) {
                    return std::nullopt;
                }

                EvalValue out;
                out.kind = ValueKind::Text;
                if (call->kind == ICallKind::Oconv) {
                    if (const std::optional<std::string> converted =
                            Conversion::oconvOutput(input, *code, error)) {
                        out.text = *converted;
                        return out;
                    }
                    return std::nullopt;
                }
                if (const std::optional<std::string> converted = Conversion::iconvOutput(input, *code, error)) {
                    out.text = *converted;
                    return out;
                }
                return std::nullopt;
            }
            error = "I-type: invalid expression";
            return std::nullopt;
        }

        std::optional<std::string> evalValueToString(const EvalValue &value) {
            if (value.kind == ValueKind::Text) {
                return value.text;
            }
            return formatNumericResult(value.number);
        }
    } // namespace

    std::optional<std::string> IExpressionEvaluator::evaluate(const IExpr &expr,
                                                              const PickFS::StructuredRecord &dataRecord,
                                                              std::string &error) {
        error.clear();
        const std::optional<EvalValue> value = evaluateValue(expr, dataRecord, error);
        if (!value.has_value()) {
            return std::nullopt;
        }
        return evalValueToString(*value);
    }
} // namespace PickCore::English
