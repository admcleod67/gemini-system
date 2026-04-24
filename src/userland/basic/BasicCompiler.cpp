#include "BasicCompiler.h"
#include "BasicExpressionParser.h"

#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
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

        std::optional<std::string> parseStringLiteral(const std::string &expr) {
            if (expr.size() >= 2 && expr.front() == '"' && expr.back() == '"') {
                return expr.substr(1, expr.size() - 2);
            }
            return std::nullopt;
        }

        PickVM::Instruction makeNoOperandInstruction(const PickVM::OpCode op) {
            return PickVM::Instruction{op, PickVM::Value{}};
        }

        class ExpressionAstEmitter {
        public:
            ExpressionAstEmitter(std::vector<PickVM::Instruction> &out, std::string &error)
                : out_(out), error_(error) {
            }

            [[nodiscard]] bool emit(const BasicAst::Expr &expression) {
                if (!emitNode(expression)) {
                    if (error_.empty()) {
                        error_ = "Failed to emit expression";
                    }
                    return false;
                }
                return true;
            }

        private:
            std::vector<PickVM::Instruction> &out_;
            std::string &error_;

            void emitNoOperand(PickVM::OpCode op) const {
                out_.push_back(makeNoOperandInstruction(op));
            }

            static PickVM::OpCode toOpCode(const BasicAst::BinaryOp op) {
                switch (op) {
                    case BasicAst::BinaryOp::Add: return PickVM::OpCode::Add;
                    case BasicAst::BinaryOp::Subtract: return PickVM::OpCode::Sub;
                    case BasicAst::BinaryOp::Multiply: return PickVM::OpCode::Mul;
                    case BasicAst::BinaryOp::Divide: return PickVM::OpCode::Div;
                    case BasicAst::BinaryOp::Equal: return PickVM::OpCode::Eq;
                    case BasicAst::BinaryOp::NotEqual: return PickVM::OpCode::Ne;
                    case BasicAst::BinaryOp::LessThan: return PickVM::OpCode::Lt;
                    case BasicAst::BinaryOp::LessOrEqual: return PickVM::OpCode::Le;
                    case BasicAst::BinaryOp::GreaterThan: return PickVM::OpCode::Gt;
                    case BasicAst::BinaryOp::GreaterOrEqual: return PickVM::OpCode::Ge;
                }
                return PickVM::OpCode::Add;
            }

            bool emitNode(const BasicAst::Expr &expression) {
                return std::visit(
                    [this](const auto &node) -> bool {
                        using NodeT = std::decay_t<decltype(node)>;
                        if constexpr (std::is_same_v<NodeT, BasicAst::IntLiteralExpr>) {
                            out_.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, node.value});
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::IdentifierExpr>) {
                            if (!isValidVariableName(node.name)) {
                                error_ = "Invalid variable name '" + node.name + "'";
                                return false;
                            }
                            out_.push_back(PickVM::Instruction{PickVM::OpCode::LoadVar, uppercase(node.name)});
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::UnaryExpr>) {
                            if (node.op != BasicAst::UnaryOp::Negate || !node.operand) {
                                error_ = "Invalid unary expression";
                                return false;
                            }
                            out_.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, 0});
                            if (!emitNode(*node.operand)) {
                                return false;
                            }
                            emitNoOperand(PickVM::OpCode::Sub);
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::BinaryExpr>) {
                            if (!node.left || !node.right) {
                                error_ = "Invalid binary expression";
                                return false;
                            }
                            if (!emitNode(*node.left) || !emitNode(*node.right)) {
                                return false;
                            }
                            emitNoOperand(toOpCode(node.op));
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::GroupedExpr>) {
                            if (!node.expression) {
                                error_ = "Invalid grouped expression";
                                return false;
                            }
                            return emitNode(*node.expression);
                        }
                        return false;
                    },
                    expression.node);
            }
        };

        bool emitIntegerExpression(const std::string &expr, std::vector<PickVM::Instruction> &out, std::string &error) {
            BasicExpressionParseResult parsed = BasicExpressionParser::parse(expr);
            if (!parsed.success || !parsed.expression) {
                error = parsed.error;
                if (error.empty()) {
                    error = "empty expression";
                }
                return false;
            }

            ExpressionAstEmitter emitter(out, error);
            return emitter.emit(*parsed.expression);
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
                bool suppressEol = false;
                if (!expr.empty() && expr.back() == ';') {
                    suppressEol = true;
                    expr = trim(expr.substr(0, expr.size() - 1));
                }
                if (expr.empty()) {
                    result.errors.push_back({lineNumber, "PRINT requires an expression"});
                    continue;
                }

                const std::optional<std::string> asString = parseStringLiteral(expr);
                if (asString) {
                    result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushStr, *asString});
                    result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintStr));
                    if (!suppressEol) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintEol));
                    }
                    continue;
                }

                std::string error;
                if (!emitIntegerExpression(expr, result.program, error)) {
                    result.errors.push_back({lineNumber, "PRINT expression error: " + error});
                    continue;
                }
                result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintInt));
                if (!suppressEol) {
                    result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintEol));
                }
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
