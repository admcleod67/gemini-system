#include "BasicExpressionParser.h"

#include "BasicBuiltinNames.h"
#include "BasicCompileUtils.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace PickShell {
    namespace {
        enum class ExprTokenType {
            IntLiteral,
            FloatLiteral,
            StringLiteral,
            Identifier,
            Plus,
            Minus,
            Star,
            Slash,
            Eq,
            Ne,
            Lt,
            Le,
            Gt,
            Ge,
            LParen,
            RParen,
            Comma,
            End,
        };

        struct ExprToken {
            ExprTokenType type{ExprTokenType::End};
            std::string lexeme;
            int intValue{0};
            BasicAst::SourceRange range{};
            std::string strValue{}; // populated for StringLiteral tokens
            double floatValue{0.0};
        };

        bool parseIntLiteral(const std::string &token, int &value) {
            if (token.empty()) {
                return false;
            }
            char *end = nullptr;
            errno = 0;
            const long v = std::strtol(token.c_str(), &end, 10);
            if (errno != 0 || end == nullptr || *end != '\0') {
                return false;
            }
            if (v < static_cast<long>(std::numeric_limits<int>::min()) ||
                v > static_cast<long>(std::numeric_limits<int>::max())) {
                return false;
            }
            value = static_cast<int>(v);
            return true;
        }

        bool parseFloatLiteral(const std::string &token, double &value) {
            if (token.empty()) {
                return false;
            }
            char *end = nullptr;
            errno = 0;
            const double v = std::strtod(token.c_str(), &end);
            if (errno != 0 || end == nullptr || *end != '\0') {
                return false;
            }
            value = v;
            return true;
        }

        std::optional<std::vector<ExprToken> > tokenizeExpression(
            const std::string_view expressionText,
            std::string &error) {
            std::vector<ExprToken> tokens;
            std::size_t i = 0;
            while (i < expressionText.size()) {
                const char c = expressionText[i];
                if (std::isspace(static_cast<unsigned char>(c)) != 0) {
                    ++i;
                    continue;
                }

                if (c == '"') {
                    const std::size_t start = i;
                    ++i; // skip opening quote
                    std::string value;
                    while (i < expressionText.size() && expressionText[i] != '"') {
                        value += expressionText[i];
                        ++i;
                    }
                    if (i >= expressionText.size()) {
                        error = "Unterminated string literal";
                        return std::nullopt;
                    }
                    ++i; // skip closing quote
                    ExprToken tok;
                    tok.type = ExprTokenType::StringLiteral;
                    tok.lexeme = std::string(expressionText.substr(start, i - start));
                    tok.strValue = value;
                    tok.range = {start, i};
                    tokens.push_back(std::move(tok));
                    continue;
                }

                if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
                    const std::size_t start = i;
                    while (i < expressionText.size() &&
                           std::isdigit(static_cast<unsigned char>(expressionText[i])) != 0) {
                        ++i;
                    }
                    bool isFloat = false;
                    if (i < expressionText.size() && expressionText[i] == '.') {
                        isFloat = true;
                        ++i;
                        while (i < expressionText.size() &&
                               std::isdigit(static_cast<unsigned char>(expressionText[i])) != 0) {
                            ++i;
                        }
                    }
                    if (i < expressionText.size() && (expressionText[i] == 'E' || expressionText[i] == 'e')) {
                        isFloat = true;
                        ++i;
                        if (i < expressionText.size() && (expressionText[i] == '+' || expressionText[i] == '-')) {
                            ++i;
                        }
                        const std::size_t expStart = i;
                        while (i < expressionText.size() &&
                               std::isdigit(static_cast<unsigned char>(expressionText[i])) != 0) {
                            ++i;
                        }
                        if (i == expStart) {
                            error = "Invalid float literal";
                            return std::nullopt;
                        }
                    }
                    const std::string literal = std::string(expressionText.substr(start, i - start));
                    if (isFloat) {
                        double parsed = 0.0;
                        if (!parseFloatLiteral(literal, parsed)) {
                            error = "Invalid float literal '" + literal + "'";
                            return std::nullopt;
                        }
                        ExprToken tok{};
                        tok.type = ExprTokenType::FloatLiteral;
                        tok.lexeme = literal;
                        tok.floatValue = parsed;
                        tok.range = {start, i};
                        tokens.push_back(std::move(tok));
                    } else {
                        int parsed = 0;
                        if (!parseIntLiteral(literal, parsed)) {
                            error = "Invalid integer literal '" + literal + "'";
                            return std::nullopt;
                        }
                        ExprToken tok{};
                        tok.type = ExprTokenType::IntLiteral;
                        tok.lexeme = literal;
                        tok.intValue = parsed;
                        tok.range = {start, i};
                        tokens.push_back(std::move(tok));
                    }
                    continue;
                }

                if (c == '@') {
                    const std::size_t start = i;
                    ++i;
                    if (i >= expressionText.size() ||
                        std::isalpha(static_cast<unsigned char>(expressionText[i])) == 0) {
                        error = "Invalid identifier after '@'";
                        return std::nullopt;
                    }
                    while (i < expressionText.size() &&
                           std::isalnum(static_cast<unsigned char>(expressionText[i])) != 0) {
                        ++i;
                    }
                    const std::string ident = std::string(expressionText.substr(start, i - start));
                    if (!isReadonlySessionSystemVariable(ident)) {
                        error = "Unknown system variable '" + ident + "'";
                        return std::nullopt;
                    }
                    tokens.push_back({ExprTokenType::Identifier, ident, 0, {start, i}, {}});
                    continue;
                }

                if (std::isalpha(static_cast<unsigned char>(c)) != 0) {
                    const std::size_t start = i;
                    while (i < expressionText.size() &&
                           std::isalnum(static_cast<unsigned char>(expressionText[i])) != 0) {
                        ++i;
                    }
                    // Allow optional $ or % suffix for string/integer variables (e.g. A$, NAME$, A%).
                    if (i < expressionText.size() &&
                        (expressionText[i] == '$' || expressionText[i] == '%')) {
                        ++i;
                    }
                    const std::string ident = std::string(expressionText.substr(start, i - start));
                    tokens.push_back({ExprTokenType::Identifier, ident, 0, {start, i}, {}});
                    continue;
                }

                if (c == '<' && i + 1 < expressionText.size() && expressionText[i + 1] == '=') {
                    tokens.push_back({ExprTokenType::Le, "<=", 0, {i, i + 2}, {}});
                    i += 2;
                    continue;
                }
                if (c == '>' && i + 1 < expressionText.size() && expressionText[i + 1] == '=') {
                    tokens.push_back({ExprTokenType::Ge, ">=", 0, {i, i + 2}, {}});
                    i += 2;
                    continue;
                }
                if (c == '<' && i + 1 < expressionText.size() && expressionText[i + 1] == '>') {
                    const bool adjacentIdentifierBefore = !tokens.empty() &&
                                                          tokens.back().type == ExprTokenType::Identifier &&
                                                          tokens.back().range.end == i;
                    if (adjacentIdentifierBefore) {
                        tokens.push_back({ExprTokenType::Lt, "<", 0, {i, i + 1}, {}});
                        ++i;
                    } else {
                        tokens.push_back({ExprTokenType::Ne, "<>", 0, {i, i + 2}, {}});
                        i += 2;
                    }
                    continue;
                }

                ExprTokenType type = ExprTokenType::End;
                switch (c) {
                    case '+': type = ExprTokenType::Plus;
                        break;
                    case '-': type = ExprTokenType::Minus;
                        break;
                    case '*': type = ExprTokenType::Star;
                        break;
                    case '/': type = ExprTokenType::Slash;
                        break;
                    case '=': type = ExprTokenType::Eq;
                        break;
                    case '<': type = ExprTokenType::Lt;
                        break;
                    case '>': type = ExprTokenType::Gt;
                        break;
                    case '(': type = ExprTokenType::LParen;
                        break;
                    case ')': type = ExprTokenType::RParen;
                        break;
                    case ',': type = ExprTokenType::Comma;
                        break;
                    default:
                        error = "Unexpected token '" + std::string(1, c) + "'";
                        return std::nullopt;
                }
                tokens.push_back({type, std::string(1, c), 0, {i, i + 1}, {}});
                ++i;
            }

            tokens.push_back({ExprTokenType::End, "", 0, {expressionText.size(), expressionText.size()}, {}});
            return tokens;
        }

        class ExpressionParser {
        public:
            explicit ExpressionParser(std::vector<ExprToken> tokens)
                : tokens_(std::move(tokens)) {
            }

            [[nodiscard]] BasicExpressionParseResult parse() {
                BasicExpressionParseResult result;
                if (tokens_.size() == 1 && tokens_[0].type == ExprTokenType::End) {
                    result.error = "empty expression";
                    return result;
                }

                std::unique_ptr<BasicAst::Expr> parsed = parseComparison();
                if (!parsed) {
                    result.error = error_;
                    return result;
                }
                if (current().type != ExprTokenType::End) {
                    result.error = "Unexpected token '" + current().lexeme + "'";
                    return result;
                }

                result.success = true;
                result.expression = std::move(parsed);
                return result;
            }

        private:
            std::vector<ExprToken> tokens_;
            std::size_t pos_{0};
            std::string error_;

            [[nodiscard]] const ExprToken &current() const {
                return tokens_[pos_];
            }

            const ExprToken &advance() {
                const ExprToken &tok = current();
                if (pos_ < tokens_.size()) {
                    ++pos_;
                }
                return tok;
            }

            static BasicAst::BinaryOp toBinaryOp(const ExprTokenType type) {
                switch (type) {
                    case ExprTokenType::Plus: return BasicAst::BinaryOp::Add;
                    case ExprTokenType::Minus: return BasicAst::BinaryOp::Subtract;
                    case ExprTokenType::Star: return BasicAst::BinaryOp::Multiply;
                    case ExprTokenType::Slash: return BasicAst::BinaryOp::Divide;
                    case ExprTokenType::Eq: return BasicAst::BinaryOp::Equal;
                    case ExprTokenType::Ne: return BasicAst::BinaryOp::NotEqual;
                    case ExprTokenType::Lt: return BasicAst::BinaryOp::LessThan;
                    case ExprTokenType::Le: return BasicAst::BinaryOp::LessOrEqual;
                    case ExprTokenType::Gt: return BasicAst::BinaryOp::GreaterThan;
                    case ExprTokenType::Ge: return BasicAst::BinaryOp::GreaterOrEqual;
                    default: return BasicAst::BinaryOp::Add;
                }
            }

            static bool isComparisonToken(const ExprTokenType type) {
                return type == ExprTokenType::Eq || type == ExprTokenType::Ne ||
                       type == ExprTokenType::Lt || type == ExprTokenType::Le ||
                       type == ExprTokenType::Gt || type == ExprTokenType::Ge;
            }

            static std::unique_ptr<BasicAst::Expr> makeExpr(BasicAst::ExprNode node, const BasicAst::SourceRange range) {
                auto expr = std::make_unique<BasicAst::Expr>();
                expr->node = std::move(node);
                expr->range = range;
                return expr;
            }

            std::unique_ptr<BasicAst::Expr> parseComparison() {
                std::unique_ptr<BasicAst::Expr> left = parseAddSub();
                if (!left) {
                    return nullptr;
                }

                if (!isComparisonToken(current().type)) {
                    return left;
                }

                const ExprToken opToken = advance();
                std::unique_ptr<BasicAst::Expr> right = parseAddSub();
                if (!right) {
                    if (error_.empty()) {
                        error_ = "Invalid operand/operator placement near '" + opToken.lexeme + "'";
                    }
                    return nullptr;
                }

                const BasicAst::SourceRange range{left->range.begin, right->range.end};
                BasicAst::BinaryExpr binary{
                    toBinaryOp(opToken.type),
                    std::move(left),
                    std::move(right),
                    range
                };
                return makeExpr(std::move(binary), range);
            }

            std::unique_ptr<BasicAst::Expr> parseAddSub() {
                std::unique_ptr<BasicAst::Expr> left = parseMulDiv();
                if (!left) {
                    return nullptr;
                }

                while (current().type == ExprTokenType::Plus || current().type == ExprTokenType::Minus) {
                    const ExprToken opToken = advance();
                    std::unique_ptr<BasicAst::Expr> right = parseMulDiv();
                    if (!right) {
                        if (error_.empty()) {
                            error_ = "Invalid operand/operator placement near '" + opToken.lexeme + "'";
                        }
                        return nullptr;
                    }

                    const BasicAst::SourceRange range{left->range.begin, right->range.end};
                    BasicAst::BinaryExpr binary{
                        toBinaryOp(opToken.type),
                        std::move(left),
                        std::move(right),
                        range
                    };
                    left = makeExpr(std::move(binary), range);
                }
                return left;
            }

            std::unique_ptr<BasicAst::Expr> parseMulDiv() {
                std::unique_ptr<BasicAst::Expr> left = parseUnary();
                if (!left) {
                    return nullptr;
                }

                while (current().type == ExprTokenType::Star || current().type == ExprTokenType::Slash) {
                    const ExprToken opToken = advance();
                    std::unique_ptr<BasicAst::Expr> right = parseUnary();
                    if (!right) {
                        if (error_.empty()) {
                            error_ = "Invalid operand/operator placement near '" + opToken.lexeme + "'";
                        }
                        return nullptr;
                    }

                    const BasicAst::SourceRange range{left->range.begin, right->range.end};
                    BasicAst::BinaryExpr binary{
                        toBinaryOp(opToken.type),
                        std::move(left),
                        std::move(right),
                        range
                    };
                    left = makeExpr(std::move(binary), range);
                }
                return left;
            }

            std::unique_ptr<BasicAst::Expr> parseUnary() {
                if (current().type == ExprTokenType::Minus) {
                    const ExprToken minusToken = advance();
                    std::unique_ptr<BasicAst::Expr> operand = parseUnary();
                    if (!operand) {
                        if (error_.empty()) {
                            error_ = "Invalid operand/operator placement near '-'";
                        }
                        return nullptr;
                    }

                    const BasicAst::SourceRange range{minusToken.range.begin, operand->range.end};
                    BasicAst::UnaryExpr unary{BasicAst::UnaryOp::Negate, std::move(operand), range};
                    return makeExpr(std::move(unary), range);
                }

                return parsePrimary();
            }

            std::unique_ptr<BasicAst::Expr> parsePrimary() {
                if (current().type == ExprTokenType::IntLiteral) {
                    const ExprToken tok = advance();
                    BasicAst::IntLiteralExpr node{tok.intValue, tok.range};
                    return makeExpr(std::move(node), tok.range);
                }

                if (current().type == ExprTokenType::FloatLiteral) {
                    const ExprToken tok = advance();
                    BasicAst::FloatLiteralExpr node{tok.floatValue, tok.range};
                    return makeExpr(std::move(node), tok.range);
                }

                if (current().type == ExprTokenType::StringLiteral) {
                    const ExprToken tok = advance();
                    BasicAst::StringLiteralExpr node{tok.strValue, tok.range};
                    return makeExpr(std::move(node), tok.range);
                }

                if (current().type == ExprTokenType::Identifier) {
                    const ExprToken tok = advance();

                    if (current().type == ExprTokenType::LParen) {
                        // Uppercase the identifier to check for builtin functions.
                        std::string upper = tok.lexeme;
                        for (char &c : upper) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }

                        if (BasicBuiltins::isBuiltinCallName(upper)) {
                            advance(); // consume '('
                            std::vector<std::unique_ptr<BasicAst::Expr>> args;
                            if (current().type == ExprTokenType::RParen) {
                                error_ = upper + " requires an argument";
                                return nullptr;
                            }
                            for (;;) {
                                std::unique_ptr<BasicAst::Expr> arg = parseComparison();
                                if (!arg) {
                                    return nullptr;
                                }
                                args.push_back(std::move(arg));
                                if (current().type == ExprTokenType::Comma) {
                                    advance();
                                    if (current().type == ExprTokenType::RParen) {
                                        error_ = "Trailing comma in " + upper + " argument list";
                                        return nullptr;
                                    }
                                    continue;
                                }
                                break;
                            }
                            if (current().type != ExprTokenType::RParen) {
                                error_ = "Missing ')' after " + upper + " argument";
                                return nullptr;
                            }
                            const std::optional<int> expectedArity = BasicBuiltins::arityForBuiltinCall(upper);
                            if (expectedArity.has_value() &&
                                static_cast<int>(args.size()) != *expectedArity) {
                                error_ = upper + " expects " + std::to_string(*expectedArity) + " argument(s), got " +
                                         std::to_string(args.size());
                                return nullptr;
                            }
                            const ExprToken close = advance();
                            const BasicAst::SourceRange range{tok.range.begin, close.range.end};
                            BasicAst::FunctionCallExpr node{upper, std::move(args), range};
                            return makeExpr(std::move(node), range);
                        }

                        // Array subscript: Identifier followed by '('
                        advance(); // consume '('
                        auto idx = parseAddSub();
                        if (!idx) {
                            return nullptr;
                        }
                        if (current().type != ExprTokenType::RParen) {
                            error_ = "Missing ')' in array subscript";
                            return nullptr;
                        }
                        const ExprToken close = advance();
                        const BasicAst::SourceRange range{tok.range.begin, close.range.end};
                        BasicAst::SubscriptExpr node{tok.lexeme, std::move(idx), range};
                        return makeExpr(std::move(node), range);
                    }

                    // Multi-value attribute read sugar: IDENT<attr[,valueIndex]>.
                    // Only treat '<' as attribute access when immediately adjacent
                    // to the identifier token (IDENT<...), keeping "IDENT < 1" as comparison.
                    if (current().type == ExprTokenType::Lt && current().range.begin == tok.range.end) {
                        advance(); // consume '<'
                        if (current().type == ExprTokenType::Comma || current().type == ExprTokenType::Gt ||
                            current().type == ExprTokenType::End) {
                            error_ = "Attribute index cannot be empty";
                            return nullptr;
                        }
                        std::unique_ptr<BasicAst::Expr> attrExpr = parseAddSub();
                        if (!attrExpr) {
                            if (error_.empty()) {
                                error_ = "Attribute index cannot be empty";
                            }
                            return nullptr;
                        }
                        std::unique_ptr<BasicAst::Expr> valueIndexExpr;
                        if (current().type == ExprTokenType::Comma) {
                            advance(); // consume ','
                            if (current().type == ExprTokenType::Gt || current().type == ExprTokenType::End) {
                                error_ = "Value index cannot be empty";
                                return nullptr;
                            }
                            valueIndexExpr = parseAddSub();
                            if (!valueIndexExpr) {
                                if (error_.empty()) {
                                    error_ = "Value index cannot be empty";
                                }
                                return nullptr;
                            }
                        }
                        if (current().type != ExprTokenType::Gt) {
                            error_ = "Missing '>' in attribute access";
                            return nullptr;
                        }
                        const ExprToken close = advance();
                        const BasicAst::SourceRange range{tok.range.begin, close.range.end};
                        BasicAst::AttributeAccessExpr node{
                            tok.lexeme,
                            std::move(attrExpr),
                            std::move(valueIndexExpr),
                            range
                        };
                        return makeExpr(std::move(node), range);
                    }
                    BasicAst::IdentifierExpr node{tok.lexeme, tok.range};
                    return makeExpr(std::move(node), tok.range);
                }

                if (current().type == ExprTokenType::LParen) {
                    const ExprToken open = advance();
                    if (current().type == ExprTokenType::RParen) {
                        error_ = "empty expression";
                        return nullptr;
                    }

                    // Preserve previous behavior: parentheses do not allow nested comparison operators.
                    std::unique_ptr<BasicAst::Expr> inner = parseAddSub();
                    if (!inner) {
                        return nullptr;
                    }
                    if (current().type != ExprTokenType::RParen) {
                        error_ = "Missing ')'";
                        return nullptr;
                    }
                    const ExprToken close = advance();
                    const BasicAst::SourceRange range{open.range.begin, close.range.end};
                    BasicAst::GroupedExpr grouped{std::move(inner), range};
                    return makeExpr(std::move(grouped), range);
                }

                if (current().type == ExprTokenType::End) {
                    error_ = "empty expression";
                } else {
                    error_ = "Unexpected token '" + current().lexeme + "'";
                }
                return nullptr;
            }
        };
    } // namespace

    BasicExpressionParseResult BasicExpressionParser::parse(const std::string_view expressionText) {
        BasicExpressionParseResult result;
        std::string error;
        const std::optional<std::vector<ExprToken> > maybeTokens = tokenizeExpression(expressionText, error);
        if (!maybeTokens) {
            result.error = std::move(error);
            return result;
        }

        ExpressionParser parser(*maybeTokens);
        return parser.parse();
    }
} // namespace PickShell
