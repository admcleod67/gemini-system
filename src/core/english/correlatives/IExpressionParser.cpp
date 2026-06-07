#include "IExpressionParser.h"

#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

namespace PickCore::English {
    namespace {
        enum class TokenKind {
            End,
            Number,
            String,
            Field,
            KwIf,
            KwThen,
            KwElse,
            KwIconv,
            KwOconv,
            Plus,
            Minus,
            Star,
            Slash,
            LParen,
            RParen,
            Comma,
            Eq,
            Ne,
            Lt,
            Le,
            Gt,
            Ge,
            Invalid,
        };

        struct Token {
            TokenKind kind{TokenKind::End};
            std::string lexeme;
            double numberValue{0};
            int fieldAttributeNo{0};
        };

        std::string upperAscii(std::string text) {
            for (char &c: text) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return text;
        }

        class Lexer {
        public:
            explicit Lexer(std::string_view source) : source_(source) {}

            Token next() {
                skipWhitespace();
                if (pos_ >= source_.size()) {
                    return Token{TokenKind::End, {}};
                }

                const char c = source_[pos_];

                if (c == '"') {
                    return lexString();
                }

                if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
                    return lexNumber();
                }

                if (std::isalpha(static_cast<unsigned char>(c))) {
                    return lexIdentifier();
                }

                if (c == '<') {
                    if (pos_ + 1U < source_.size() && source_[pos_ + 1U] == '>') {
                        pos_ += 2;
                        return Token{TokenKind::Ne, "<>"};
                    }
                    if (pos_ + 1U < source_.size() && source_[pos_ + 1U] == '=') {
                        pos_ += 2;
                        return Token{TokenKind::Le, "<="};
                    }
                    ++pos_;
                    return Token{TokenKind::Lt, "<"};
                }

                if (c == '>') {
                    if (pos_ + 1U < source_.size() && source_[pos_ + 1U] == '=') {
                        pos_ += 2;
                        return Token{TokenKind::Ge, ">="};
                    }
                    ++pos_;
                    return Token{TokenKind::Gt, ">"};
                }

                if (c == '=') {
                    ++pos_;
                    return Token{TokenKind::Eq, "="};
                }

                if (c == '#') {
                    ++pos_;
                    return Token{TokenKind::Ne, "#"};
                }

                ++pos_;
                switch (c) {
                    case '+':
                        return Token{TokenKind::Plus, "+"};
                    case '-':
                        return Token{TokenKind::Minus, "-"};
                    case '*':
                        return Token{TokenKind::Star, "*"};
                    case '/':
                        return Token{TokenKind::Slash, "/"};
                    case '(':
                        return Token{TokenKind::LParen, "("};
                    case ')':
                        return Token{TokenKind::RParen, ")"};
                    case ',':
                        return Token{TokenKind::Comma, ","};
                    default:
                        return Token{TokenKind::Invalid, std::string(1, c)};
                }
            }

        private:
            void skipWhitespace() {
                while (pos_ < source_.size() &&
                       std::isspace(static_cast<unsigned char>(source_[pos_]))) {
                    ++pos_;
                }
            }

            Token lexString() {
                ++pos_;
                std::string value;
                while (pos_ < source_.size()) {
                    const char c = source_[pos_++];
                    if (c == '"') {
                        Token tok;
                        tok.kind = TokenKind::String;
                        tok.lexeme = value;
                        return tok;
                    }
                    if (c == '\\' && pos_ < source_.size()) {
                        value.push_back(source_[pos_++]);
                        continue;
                    }
                    value.push_back(c);
                }
                return Token{TokenKind::Invalid, value};
            }

            Token lexNumber() {
                const std::size_t start = pos_;
                bool sawDot = false;
                while (pos_ < source_.size()) {
                    const char ch = source_[pos_];
                    if (std::isdigit(static_cast<unsigned char>(ch))) {
                        ++pos_;
                        continue;
                    }
                    if (ch == '.' && !sawDot) {
                        sawDot = true;
                        ++pos_;
                        continue;
                    }
                    break;
                }

                const std::string lexeme(source_.substr(start, pos_ - start));
                char *end = nullptr;
                const double value = std::strtod(lexeme.c_str(), &end);
                if (end == nullptr || *end != '\0') {
                    return Token{TokenKind::Invalid, lexeme};
                }
                Token tok;
                tok.kind = TokenKind::Number;
                tok.lexeme = lexeme;
                tok.numberValue = value;
                return tok;
            }

            Token lexIdentifier() {
                const std::size_t start = pos_;
                while (pos_ < source_.size() &&
                       std::isalpha(static_cast<unsigned char>(source_[pos_]))) {
                    ++pos_;
                }
                const std::string lexeme(source_.substr(start, pos_ - start));
                const std::string upper = upperAscii(lexeme);

                if (upper == "IF") {
                    return Token{TokenKind::KwIf, lexeme};
                }
                if (upper == "THEN") {
                    return Token{TokenKind::KwThen, lexeme};
                }
                if (upper == "ELSE") {
                    return Token{TokenKind::KwElse, lexeme};
                }
                if (upper == "ICONV") {
                    return Token{TokenKind::KwIconv, lexeme};
                }
                if (upper == "OCONV") {
                    return Token{TokenKind::KwOconv, lexeme};
                }

                if (lexeme.size() == 1) {
                    const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(lexeme[0])));
                    if (letter >= 'A' && letter <= 'Z') {
                        Token tok;
                        tok.kind = TokenKind::Field;
                        tok.lexeme = lexeme;
                        tok.fieldAttributeNo = static_cast<int>(letter - 'A') + 1;
                        return tok;
                    }
                }

                return Token{TokenKind::Invalid, lexeme};
            }

            std::string_view source_;
            std::size_t pos_{0};
        };

        bool isCompareToken(const TokenKind kind) {
            switch (kind) {
                case TokenKind::Eq:
                case TokenKind::Ne:
                case TokenKind::Lt:
                case TokenKind::Le:
                case TokenKind::Gt:
                case TokenKind::Ge:
                    return true;
                default:
                    return false;
            }
        }

        ICompareOp compareOpFromToken(const TokenKind kind) {
            switch (kind) {
                case TokenKind::Eq:
                    return ICompareOp::Eq;
                case TokenKind::Ne:
                    return ICompareOp::Ne;
                case TokenKind::Lt:
                    return ICompareOp::Lt;
                case TokenKind::Le:
                    return ICompareOp::Le;
                case TokenKind::Gt:
                    return ICompareOp::Gt;
                case TokenKind::Ge:
                    return ICompareOp::Ge;
                default:
                    return ICompareOp::Eq;
            }
        }

        class Parser {
        public:
            explicit Parser(std::string_view source) : lexer_(source) { advance(); }

            std::unique_ptr<IExpr> parseExpression(std::string &error) {
                auto expr = parseExpr(error);
                if (!expr) {
                    return nullptr;
                }
                if (current_.kind != TokenKind::End) {
                    error = "I-type: invalid expression";
                    return nullptr;
                }
                return expr;
            }

        private:
            void advance() { current_ = lexer_.next(); }

            std::unique_ptr<IExpr> parseExpr(std::string &error) {
                if (current_.kind == TokenKind::KwIf) {
                    advance();
                    auto condition = parseComparison(error);
                    if (!condition) {
                        return nullptr;
                    }
                    if (current_.kind != TokenKind::KwThen) {
                        error = "I-type: invalid expression";
                        return nullptr;
                    }
                    advance();
                    auto thenExpr = parseExpr(error);
                    if (!thenExpr) {
                        return nullptr;
                    }
                    if (current_.kind != TokenKind::KwElse) {
                        error = "I-type: invalid expression";
                        return nullptr;
                    }
                    advance();
                    auto elseExpr = parseExpr(error);
                    if (!elseExpr) {
                        return nullptr;
                    }
                    return IExpr::makeIf(std::move(condition), std::move(thenExpr), std::move(elseExpr));
                }
                return parseAdditive(error);
            }

            std::unique_ptr<IExpr> parseComparison(std::string &error) {
                auto left = parseAdditive(error);
                if (!left) {
                    return nullptr;
                }
                if (isCompareToken(current_.kind)) {
                    const ICompareOp op = compareOpFromToken(current_.kind);
                    advance();
                    auto right = parseAdditive(error);
                    if (!right) {
                        return nullptr;
                    }
                    return IExpr::makeCompare(op, std::move(left), std::move(right));
                }
                return left;
            }

            std::unique_ptr<IExpr> parseAdditive(std::string &error) {
                auto left = parseMultiplicative(error);
                if (!left) {
                    return nullptr;
                }
                while (current_.kind == TokenKind::Plus || current_.kind == TokenKind::Minus) {
                    const char op = current_.lexeme[0];
                    advance();
                    auto right = parseMultiplicative(error);
                    if (!right) {
                        return nullptr;
                    }
                    left = IExpr::makeBinary(op, std::move(left), std::move(right));
                }
                return left;
            }

            std::unique_ptr<IExpr> parseMultiplicative(std::string &error) {
                auto left = parseUnary(error);
                if (!left) {
                    return nullptr;
                }
                while (current_.kind == TokenKind::Star || current_.kind == TokenKind::Slash) {
                    const char op = current_.lexeme[0];
                    advance();
                    auto right = parseUnary(error);
                    if (!right) {
                        return nullptr;
                    }
                    left = IExpr::makeBinary(op, std::move(left), std::move(right));
                }
                return left;
            }

            std::unique_ptr<IExpr> parseUnary(std::string &error) {
                if (current_.kind == TokenKind::Minus) {
                    advance();
                    auto operand = parseUnary(error);
                    if (!operand) {
                        return nullptr;
                    }
                    return IExpr::makeUnary('-', std::move(operand));
                }
                if (current_.kind == TokenKind::Plus) {
                    advance();
                    return parseUnary(error);
                }
                return parsePrimary(error);
            }

            std::unique_ptr<IExpr> parsePrimary(std::string &error) {
                if (current_.kind == TokenKind::Number) {
                    const double value = current_.numberValue;
                    advance();
                    return IExpr::makeNumber(value);
                }
                if (current_.kind == TokenKind::String) {
                    const std::string value = current_.lexeme;
                    advance();
                    return IExpr::makeString(value);
                }
                if (current_.kind == TokenKind::Field) {
                    const int attrNo = current_.fieldAttributeNo;
                    advance();
                    return IExpr::makeField(attrNo);
                }
                if (current_.kind == TokenKind::KwIconv || current_.kind == TokenKind::KwOconv) {
                    const ICallKind kind =
                        current_.kind == TokenKind::KwIconv ? ICallKind::Iconv : ICallKind::Oconv;
                    advance();
                    if (current_.kind != TokenKind::LParen) {
                        error = "I-type: invalid expression";
                        return nullptr;
                    }
                    advance();
                    auto valueArg = parseExpr(error);
                    if (!valueArg) {
                        return nullptr;
                    }
                    if (current_.kind != TokenKind::Comma) {
                        error = "I-type: invalid expression";
                        return nullptr;
                    }
                    advance();
                    auto codeArg = parseExpr(error);
                    if (!codeArg) {
                        return nullptr;
                    }
                    if (current_.kind != TokenKind::RParen) {
                        error = "I-type: invalid expression";
                        return nullptr;
                    }
                    advance();
                    return IExpr::makeCall(kind, std::move(valueArg), std::move(codeArg));
                }
                if (current_.kind == TokenKind::LParen) {
                    advance();
                    auto inner = parseExpr(error);
                    if (!inner) {
                        return nullptr;
                    }
                    if (current_.kind != TokenKind::RParen) {
                        error = "I-type: invalid expression";
                        return nullptr;
                    }
                    advance();
                    return inner;
                }
                if (current_.kind == TokenKind::Invalid) {
                    error = "I-type: invalid expression";
                    return nullptr;
                }
                error = "I-type: invalid expression";
                return nullptr;
            }

            Lexer lexer_;
            Token current_{TokenKind::End, {}};
        };
    } // namespace

    std::unique_ptr<IExpr> IExpressionParser::parse(const std::string &source, std::string &error) {
        error.clear();
        if (source.empty()) {
            error = "I-type: invalid expression";
            return nullptr;
        }
        Parser parser(source);
        return parser.parseExpression(error);
    }
} // namespace PickCore::English
