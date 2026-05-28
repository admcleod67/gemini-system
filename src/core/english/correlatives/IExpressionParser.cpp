#include "IExpressionParser.h"

#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace PickCore::English {
    namespace {
        enum class TokenKind {
            End,
            Number,
            Field,
            Plus,
            Minus,
            Star,
            Slash,
            LParen,
            RParen,
            Invalid,
        };

        struct Token {
            TokenKind kind{TokenKind::End};
            std::string lexeme;
            double numberValue{0};
            int fieldAttributeNo{0};
        };

        class Lexer {
        public:
            explicit Lexer(std::string_view source) : source_(source) {}

            Token next() {
                skipWhitespace();
                if (pos_ >= source_.size()) {
                    return Token{TokenKind::End, {}};
                }

                const char c = source_[pos_];

                if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
                    return lexNumber();
                }

                if (std::isalpha(static_cast<unsigned char>(c))) {
                    return lexIdentifier();
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
                if (lexeme.size() != 1) {
                    return Token{TokenKind::Invalid, lexeme};
                }
                const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(lexeme[0])));
                if (letter < 'A' || letter > 'Z') {
                    return Token{TokenKind::Invalid, lexeme};
                }
                Token tok;
                tok.kind = TokenKind::Field;
                tok.lexeme = lexeme;
                tok.fieldAttributeNo = static_cast<int>(letter - 'A') + 1;
                return tok;
            }

            std::string_view source_;
            std::size_t pos_{0};
        };

        class Parser {
        public:
            explicit Parser(std::string_view source) : lexer_(source) { advance(); }

            std::unique_ptr<IExpr> parseExpression(std::string &error) {
                auto expr = parseAdditive(error);
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
                if (current_.kind == TokenKind::Field) {
                    const int attrNo = current_.fieldAttributeNo;
                    advance();
                    return IExpr::makeField(attrNo);
                }
                if (current_.kind == TokenKind::LParen) {
                    advance();
                    auto inner = parseAdditive(error);
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
