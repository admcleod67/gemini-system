#include "BasicCompiler.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace PickShell {
    namespace {
        std::string trim(const std::string &raw) {
            const std::size_t first = raw.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return "";
            }
            const std::size_t last = raw.find_last_not_of(" \t\r\n");
            return raw.substr(first, last - first + 1);
        }

        std::string uppercase(std::string s) {
            for (char &c: s) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return s;
        }

        bool isValidVariableName(const std::string &token) {
            if (token.empty()) {
                return false;
            }
            const char first = token.front();
            if (std::isalpha(static_cast<unsigned char>(first)) == 0) {
                return false;
            }
            for (const char c: token) {
                if (std::isalnum(static_cast<unsigned char>(c)) == 0) {
                    return false;
                }
            }
            return true;
        }

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

        std::optional<std::string> parseStringLiteral(const std::string &expr) {
            if (expr.size() >= 2 && expr.front() == '"' && expr.back() == '"') {
                return expr.substr(1, expr.size() - 2);
            }
            return std::nullopt;
        }

        PickVM::Instruction makeNoOperandInstruction(const PickVM::OpCode op) {
            return PickVM::Instruction{op, PickVM::Value{}};
        }

        enum class ExprTokenType {
            IntLiteral,
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
        };

        std::optional<std::vector<ExprToken> > tokenizeExpression(const std::string &expr, std::string &error) {
            std::vector<ExprToken> tokens;
            std::size_t i = 0;
            while (i < expr.size()) {
                const char c = expr[i];
                if (std::isspace(static_cast<unsigned char>(c)) != 0) {
                    ++i;
                    continue;
                }

                if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
                    const std::size_t start = i;
                    while (i < expr.size() && std::isdigit(static_cast<unsigned char>(expr[i])) != 0) {
                        ++i;
                    }
                    const std::string literal = expr.substr(start, i - start);
                    int parsed = 0;
                    if (!parseIntLiteral(literal, parsed)) {
                        error = "Invalid integer literal '" + literal + "'";
                        return std::nullopt;
                    }
                    tokens.push_back({ExprTokenType::IntLiteral, literal, parsed});
                    continue;
                }

                if (std::isalpha(static_cast<unsigned char>(c)) != 0) {
                    const std::size_t start = i;
                    while (i < expr.size() && std::isalnum(static_cast<unsigned char>(expr[i])) != 0) {
                        ++i;
                    }
                    const std::string ident = expr.substr(start, i - start);
                    tokens.push_back({ExprTokenType::Identifier, ident, 0});
                    continue;
                }

                ExprTokenType type = ExprTokenType::End;
                if (c == '<' && i + 1 < expr.size() && expr[i + 1] == '=') {
                    tokens.push_back({ExprTokenType::Le, "<=", 0});
                    i += 2;
                    continue;
                }
                if (c == '>' && i + 1 < expr.size() && expr[i + 1] == '=') {
                    tokens.push_back({ExprTokenType::Ge, ">=", 0});
                    i += 2;
                    continue;
                }
                if (c == '<' && i + 1 < expr.size() && expr[i + 1] == '>') {
                    tokens.push_back({ExprTokenType::Ne, "<>", 0});
                    i += 2;
                    continue;
                }
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
                tokens.push_back({type, std::string(1, c), 0});
                ++i;
            }
            tokens.push_back({ExprTokenType::End, "", 0});
            return tokens;
        }

        class ExpressionEmitter {
        public:
            explicit ExpressionEmitter(const std::vector<ExprToken> &tokens)
                : tokens_(tokens) {
            }

            [[nodiscard]] bool emit(std::vector<PickVM::Instruction> &out, std::string &error) {
                out_ = &out;
                error_ = &error;
                if (tokens_.size() == 1 && tokens_[0].type == ExprTokenType::End) {
                    *error_ = "empty expression";
                    return false;
                }
                if (!parseComparison()) {
                    return false;
                }
                if (current().type != ExprTokenType::End) {
                    *error_ = "Unexpected token '" + current().lexeme + "'";
                    return false;
                }
                return true;
            }

        private:
            const std::vector<ExprToken> &tokens_;
            std::size_t pos_{0};
            std::vector<PickVM::Instruction> *out_{nullptr};
            std::string *error_{nullptr};

            [[nodiscard]] const ExprToken &current() const {
                return tokens_[pos_];
            }

            void advance() {
                if (pos_ < tokens_.size()) {
                    ++pos_;
                }
            }

            void emitNoOperand(PickVM::OpCode op) const {
                out_->push_back(makeNoOperandInstruction(op));
            }

            bool isComparisonToken(const ExprTokenType type) {
                return type == ExprTokenType::Eq || type == ExprTokenType::Ne ||
                       type == ExprTokenType::Lt || type == ExprTokenType::Le ||
                       type == ExprTokenType::Gt || type == ExprTokenType::Ge;
            }

            PickVM::OpCode comparisonOpCode(const ExprTokenType type) {
                switch (type) {
                    case ExprTokenType::Eq: return PickVM::OpCode::Eq;
                    case ExprTokenType::Ne: return PickVM::OpCode::Ne;
                    case ExprTokenType::Lt: return PickVM::OpCode::Lt;
                    case ExprTokenType::Le: return PickVM::OpCode::Le;
                    case ExprTokenType::Gt: return PickVM::OpCode::Gt;
                    case ExprTokenType::Ge: return PickVM::OpCode::Ge;
                    default: return PickVM::OpCode::Eq;
                }
            }

            bool parseComparison() {
                if (!parseAddSub()) {
                    return false;
                }
                if (!isComparisonToken(current().type)) {
                    return true;
                }
                const ExprTokenType compare = current().type;
                const std::string compareText = current().lexeme;
                advance();
                if (!parseAddSub()) {
                    if (error_->empty()) {
                        *error_ = "Invalid operand/operator placement near '" + compareText + "'";
                    }
                    return false;
                }
                emitNoOperand(comparisonOpCode(compare));
                return true;
            }

            bool parseAddSub() {
                if (!parseMulDiv()) {
                    return false;
                }
                while (current().type == ExprTokenType::Plus || current().type == ExprTokenType::Minus) {
                    const ExprTokenType op = current().type;
                    const std::string opText = current().lexeme;
                    advance();
                    if (!parseMulDiv()) {
                        if (error_->empty()) {
                            *error_ = "Invalid operand/operator placement near '" + opText + "'";
                        }
                        return false;
                    }
                    emitNoOperand(op == ExprTokenType::Plus ? PickVM::OpCode::Add : PickVM::OpCode::Sub);
                }
                return true;
            }

            bool parseMulDiv() {
                if (!parseUnary()) {
                    return false;
                }
                while (current().type == ExprTokenType::Star || current().type == ExprTokenType::Slash) {
                    const ExprTokenType op = current().type;
                    const std::string opText = current().lexeme;
                    advance();
                    if (!parseUnary()) {
                        if (error_->empty()) {
                            *error_ = "Invalid operand/operator placement near '" + opText + "'";
                        }
                        return false;
                    }
                    emitNoOperand(op == ExprTokenType::Star ? PickVM::OpCode::Mul : PickVM::OpCode::Div);
                }
                return true;
            }

            bool parseUnary() {
                if (current().type == ExprTokenType::Minus) {
                    advance();
                    out_->push_back(PickVM::Instruction{PickVM::OpCode::PushInt, 0});
                    if (!parseUnary()) {
                        if (error_->empty()) {
                            *error_ = "Invalid operand/operator placement near '-'";
                        }
                        return false;
                    }
                    emitNoOperand(PickVM::OpCode::Sub);
                    return true;
                }
                return parsePrimary();
            }

            bool parsePrimary() {
                if (current().type == ExprTokenType::IntLiteral) {
                    out_->push_back(PickVM::Instruction{PickVM::OpCode::PushInt, current().intValue});
                    advance();
                    return true;
                }

                if (current().type == ExprTokenType::Identifier) {
                    const std::string ident = current().lexeme;
                    if (!isValidVariableName(ident)) {
                        *error_ = "Invalid variable name '" + ident + "'";
                        return false;
                    }
                    out_->push_back(PickVM::Instruction{PickVM::OpCode::LoadVar, uppercase(ident)});
                    advance();
                    return true;
                }

                if (current().type == ExprTokenType::LParen) {
                    advance();
                    if (current().type == ExprTokenType::RParen) {
                        *error_ = "empty expression";
                        return false;
                    }
                    if (!parseAddSub()) {
                        return false;
                    }
                    if (current().type != ExprTokenType::RParen) {
                        *error_ = "Missing ')'";
                        return false;
                    }
                    advance();
                    return true;
                }

                if (current().type == ExprTokenType::End) {
                    *error_ = "empty expression";
                } else {
                    *error_ = "Unexpected token '" + current().lexeme + "'";
                }
                return false;
            }
        };

        bool emitIntegerExpression(const std::string &expr, std::vector<PickVM::Instruction> &out, std::string &error) {
            const std::optional<std::vector<ExprToken> > maybeTokens = tokenizeExpression(expr, error);
            if (!maybeTokens) {
                return false;
            }
            ExpressionEmitter emitter(*maybeTokens);
            return emitter.emit(out, error);
        }

        bool parsePositiveLineNumber(const std::string &raw, int &lineNumber) {
            const std::string token = trim(raw);
            if (token.empty()) {
                return false;
            }
            for (const char c: token) {
                if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
                    return false;
                }
            }
            try {
                lineNumber = std::stoi(token);
            } catch (const std::exception &) {
                return false;
            }
            return lineNumber > 0;
        }

        std::size_t findKeywordToken(const std::string &upperText, const std::string &keyword) {
            std::size_t pos = upperText.find(keyword);
            while (pos != std::string::npos) {
                const bool startOk = pos == 0 || std::isspace(static_cast<unsigned char>(upperText[pos - 1])) != 0;
                const std::size_t endPos = pos + keyword.size();
                const bool endOk = endPos >= upperText.size() ||
                                   std::isspace(static_cast<unsigned char>(upperText[endPos])) != 0;
                if (startOk && endOk) {
                    return pos;
                }
                pos = upperText.find(keyword, pos + 1);
            }
            return std::string::npos;
        }

        struct JumpFixup {
            int sourceLine{0};
            std::size_t instructionIndex{0};
            int targetLine{0};
        };
    } // namespace

    BasicCompileResult BasicCompiler::compile(const BasicProgram &program) const {
        BasicCompileResult result;
        bool explicitEnd = false;
        std::unordered_map<int, std::size_t> lineToInstructionIndex;
        std::vector<JumpFixup> jumpFixups;

        for (const auto &[lineNumber, sourceText]: program.lines()) {
            const std::string line = trim(sourceText);
            if (line.empty()) {
                continue;
            }
            lineToInstructionIndex[lineNumber] = result.program.size();

            std::istringstream iss(line);
            std::string keyword;
            iss >> keyword;
            const std::string op = uppercase(keyword);

            if (op == "LET") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                const std::size_t eqPos = rest.find('=');
                if (eqPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "LET requires assignment with '='"});
                    continue;
                }
                const std::string lhsRaw = trim(rest.substr(0, eqPos));
                const std::string rhsRaw = trim(rest.substr(eqPos + 1));
                if (!isValidVariableName(lhsRaw)) {
                    result.errors.push_back({lineNumber, "Invalid variable name '" + lhsRaw + "'"});
                    continue;
                }
                if (rhsRaw.empty()) {
                    result.errors.push_back({lineNumber, "LET requires an expression"});
                    continue;
                }

                std::string error;
                if (!emitIntegerExpression(rhsRaw, result.program, error)) {
                    result.errors.push_back({lineNumber, "LET expression error: " + error});
                    continue;
                }

                result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, uppercase(lhsRaw)});
                continue;
            }

            if (op == "INPUT") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (rest.empty()) {
                    result.errors.push_back({lineNumber, "INPUT requires a variable name"});
                    continue;
                }
                std::istringstream varIss(rest);
                std::vector<std::string> inputTokens;
                std::string inputTok;
                while (varIss >> inputTok) {
                    inputTokens.push_back(inputTok);
                }
                if (inputTokens.size() != 1) {
                    result.errors.push_back({lineNumber, "INPUT requires a variable name"});
                    continue;
                }
                const std::string varName = inputTokens[0];
                if (!isValidVariableName(varName)) {
                    result.errors.push_back({lineNumber, "Invalid variable name '" + varName + "'"});
                    continue;
                }
                result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::InputInt));
                result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, uppercase(varName)});
                continue;
            }

            if (op == "GOTO") {
                std::string targetRaw;
                std::getline(iss, targetRaw);
                int targetLine = 0;
                if (!parsePositiveLineNumber(targetRaw, targetLine)) {
                    result.errors.push_back({lineNumber, "GOTO requires a line number"});
                    continue;
                }
                const std::size_t jumpIp = result.program.size();
                result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                jumpFixups.push_back({lineNumber, jumpIp, targetLine});
                continue;
            }

            if (op == "IF") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (rest.empty()) {
                    result.errors.push_back({lineNumber, "IF requires THEN <line>"});
                    continue;
                }
                const std::string restUpper = uppercase(rest);
                const std::size_t thenPos = findKeywordToken(restUpper, "THEN");
                if (thenPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "IF requires THEN <line>"});
                    continue;
                }

                const std::string conditionExpr = trim(rest.substr(0, thenPos));
                if (conditionExpr.empty()) {
                    result.errors.push_back({lineNumber, "IF requires a condition expression"});
                    continue;
                }

                std::string branchPart = trim(rest.substr(thenPos + 4));
                if (branchPart.empty()) {
                    result.errors.push_back({lineNumber, "IF THEN requires a line number"});
                    continue;
                }
                const std::string branchUpper = uppercase(branchPart);
                const std::size_t elsePos = findKeywordToken(branchUpper, "ELSE");

                std::string thenRaw = branchPart;
                std::optional<std::string> elseRaw;
                if (elsePos != std::string::npos) {
                    thenRaw = trim(branchPart.substr(0, elsePos));
                    elseRaw = trim(branchPart.substr(elsePos + 4));
                } else {
                    thenRaw = trim(thenRaw);
                }

                int thenLine = 0;
                if (!parsePositiveLineNumber(thenRaw, thenLine)) {
                    result.errors.push_back({lineNumber, "IF THEN requires a line number"});
                    continue;
                }

                int elseLine = 0;
                if (elseRaw && !parsePositiveLineNumber(*elseRaw, elseLine)) {
                    result.errors.push_back({lineNumber, "IF ELSE requires a line number"});
                    continue;
                }

                std::string condError;
                if (!emitIntegerExpression(conditionExpr, result.program, condError)) {
                    result.errors.push_back({lineNumber, "IF condition error: " + condError});
                    continue;
                }

                const std::size_t jzIp = result.program.size();
                result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});

                const std::size_t thenJumpIp = result.program.size();
                result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                jumpFixups.push_back({lineNumber, thenJumpIp, thenLine});

                if (elseRaw) {
                    result.program[jzIp].operand = static_cast<int>(result.program.size());
                    const std::size_t elseJumpIp = result.program.size();
                    result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                    jumpFixups.push_back({lineNumber, elseJumpIp, elseLine});
                } else {
                    result.program[jzIp].operand = static_cast<int>(result.program.size());
                }
                continue;
            }

            if (op == "PRINT") {
                std::string expr;
                std::getline(iss, expr);
                expr = trim(expr);
                if (expr.empty()) {
                    result.errors.push_back({lineNumber, "PRINT requires an expression"});
                    continue;
                }

                const std::optional<std::string> asString = parseStringLiteral(expr);
                if (asString) {
                    result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushStr, *asString});
                    result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintStr));
                    continue;
                }

                std::string error;
                if (!emitIntegerExpression(expr, result.program, error)) {
                    result.errors.push_back({lineNumber, "PRINT expression error: " + error});
                    continue;
                }
                result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintInt));
                continue;
            }

            if (op == "STOP") {
                std::string rest;
                std::getline(iss, rest);
                if (!trim(rest).empty()) {
                    result.errors.push_back({lineNumber, "STOP takes no arguments"});
                    continue;
                }
                result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
                continue;
            }

            if (op == "END") {
                std::string rest;
                std::getline(iss, rest);
                if (!trim(rest).empty()) {
                    result.errors.push_back({lineNumber, "END takes no arguments"});
                    continue;
                }
                result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
                explicitEnd = true;
                continue;
            }

            result.errors.push_back({lineNumber, "Unknown keyword '" + op + "'"});
        }

        for (const JumpFixup &fixup: jumpFixups) {
            const auto it = lineToInstructionIndex.find(fixup.targetLine);
            if (it == lineToInstructionIndex.end()) {
                result.errors.push_back({fixup.sourceLine, "Unknown target line " + std::to_string(fixup.targetLine)});
                continue;
            }
            result.program[fixup.instructionIndex].operand = static_cast<int>(it->second);
        }

        if (!result.errors.empty()) {
            result.program.clear();
            result.instructionCount = 0;
            result.labelCount = 0;
            result.success = false;
            return result;
        }

        if (!explicitEnd || result.program.empty() || result.program.back().op != PickVM::OpCode::Halt) {
            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
        }

        result.instructionCount = result.program.size();
        result.labelCount = 0;
        result.success = true;
        return result;
    }
} // namespace PickShell
