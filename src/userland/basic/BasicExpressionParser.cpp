#include "BasicExpressionParser.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace PickShell {
    namespace {
        enum class ExprTokenType {
            IntLiteral,
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
            End,
        };

        struct ExprToken {
            ExprTokenType type{ExprTokenType::End};
            std::string lexeme;
            int intValue{0};
            BasicAst::SourceRange range{};
            std::string strValue{}; // populated for StringLiteral tokens
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

                if (std::isdigit(static_cast<unsigned char>(c)) != 0) {                    const std::size_t start = i;
                    while (i < expressionText.size() &&
                           std::isdigit(static_cast<unsigned char>(expressionText[i])) != 0) {
                        ++i;
                    }
                    const std::string literal = std::string(expressionText.substr(start, i - start));
                    int parsed = 0;
                    if (!parseIntLiteral(literal, parsed)) {
                        error = "Invalid integer literal '" + literal + "'";
                        return std::nullopt;
                    }
                    tokens.push_back({ExprTokenType::IntLiteral, literal, parsed, {start, i}, {}});
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
                    tokens.push_back({ExprTokenType::Ne, "<>", 0, {i, i + 2}, {}});
                    i += 2;
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

                if (current().type == ExprTokenType::StringLiteral) {
                    const ExprToken tok = advance();
                    BasicAst::StringLiteralExpr node{tok.strValue, tok.range};
                    return makeExpr(std::move(node), tok.range);
                }

                if (current().type == ExprTokenType::Identifier) {
                    const ExprToken tok = advance();
                    // Array subscript: Identifier followed by '('
                    if (current().type == ExprTokenType::LParen) {
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
