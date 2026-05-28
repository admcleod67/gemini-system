#include "IExpressionEvaluator.h"

#include <cmath>
#include <cctype>
#include <sstream>
#include <string>

namespace PickCore::English {
    namespace {
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

        std::optional<double> evaluateNumeric(const IExpr &expr,
                                              const PickFS::StructuredRecord &dataRecord,
                                              std::string &error) {
            if (const auto *number = std::get_if<IExprNumber>(&expr.payload)) {
                return number->value;
            }
            if (const auto *field = std::get_if<IExprField>(&expr.payload)) {
                if (!dataRecord.hasAttribute(field->attributeNo)) {
                    return 0.0;
                }
                const std::string raw = dataRecord.attribute(field->attributeNo).firstValue();
                if (const std::optional<double> n = tryCoerceNumeric(raw)) {
                    return *n;
                }
                error = "I-type: type mismatch";
                return std::nullopt;
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
                if (unary->op == '-') {
                    return -(*operand);
                }
                error = "I-type: invalid expression";
                return std::nullopt;
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
                switch (binary->op) {
                    case '+':
                        return *left + *right;
                    case '-':
                        return *left - *right;
                    case '*':
                        return *left * *right;
                    case '/':
                        if (*right == 0.0) {
                            error = "I-type: division by zero";
                            return std::nullopt;
                        }
                        return *left / *right;
                    default:
                        error = "I-type: invalid expression";
                        return std::nullopt;
                }
            }
            error = "I-type: invalid expression";
            return std::nullopt;
        }
    } // namespace

    std::optional<std::string> IExpressionEvaluator::evaluate(const IExpr &expr,
                                                              const PickFS::StructuredRecord &dataRecord,
                                                              std::string &error) {
        error.clear();
        const std::optional<double> value = evaluateNumeric(expr, dataRecord, error);
        if (!value.has_value()) {
            return std::nullopt;
        }
        return formatNumericResult(*value);
    }
} // namespace PickCore::English
