#include "BasicCompiler.h"

#include "BasicStatementParser.h"

#include <cctype>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace PickShell {
    namespace {
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

        const BasicAst::StatementParseResult parsed = BasicStatementParser::parse(program);
        if (!parsed.success) {
            result.success = false;
            result.program.clear();
            result.instructionCount = 0;
            result.labelCount = 0;
            for (const auto &error: parsed.errors) {
                result.errors.push_back({error.line, error.message});
            }
            return result;
        }

        for (const BasicAst::ParsedBasicLine &parsedLine: parsed.lines) {
            lineToInstructionIndex[parsedLine.lineNumber] = result.program.size();

            const bool emitted = std::visit(
                [&](const auto &stmt) -> bool {
                    using StmtT = std::decay_t<decltype(stmt)>;
                    if constexpr (std::is_same_v<StmtT, BasicAst::LetStmt>) {
                        std::string error;
                        if (!stmt.expression) {
                            result.errors.push_back({parsedLine.lineNumber, "LET requires an expression"});
                            return false;
                        }
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.expression)) {
                            result.errors.push_back({parsedLine.lineNumber, "LET expression error: " + error});
                            return false;
                        }
                        result.program.push_back(PickVM::Instruction{
                            PickVM::OpCode::StoreVar, uppercase(stmt.variableName)
                        });
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::InputStmt>) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::InputInt));
                        result.program.push_back(PickVM::Instruction{
                            PickVM::OpCode::StoreVar, uppercase(stmt.variableName)
                        });
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::GotoStmt>) {
                        const std::size_t jumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        jumpFixups.push_back({parsedLine.lineNumber, jumpIp, stmt.targetLine});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::IfStmt>) {
                        std::string error;
                        if (!stmt.condition) {
                            result.errors.push_back({parsedLine.lineNumber, "IF requires a condition expression"});
                            return false;
                        }
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.condition)) {
                            result.errors.push_back({parsedLine.lineNumber, "IF condition error: " + error});
                            return false;
                        }

                        const std::size_t jzIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});

                        const std::size_t thenJumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        jumpFixups.push_back({parsedLine.lineNumber, thenJumpIp, stmt.thenLine});

                        if (stmt.elseLine.has_value()) {
                            result.program[jzIp].operand = static_cast<int>(result.program.size());
                            const std::size_t elseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            jumpFixups.push_back({parsedLine.lineNumber, elseJumpIp, *stmt.elseLine});
                        } else {
                            result.program[jzIp].operand = static_cast<int>(result.program.size());
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::PrintStmt>) {
                        if (stmt.stringLiteral.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushStr, *stmt.stringLiteral});
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintStr));
                        } else {
                            std::string error;
                            if (!stmt.expression) {
                                result.errors.push_back({parsedLine.lineNumber, "PRINT requires an expression"});
                                return false;
                            }
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.expression)) {
                                result.errors.push_back({parsedLine.lineNumber, "PRINT expression error: " + error});
                                return false;
                            }
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintInt));
                        }
                        if (!stmt.suppressEol) {
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintEol));
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::StopStmt>) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::EndStmt>) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
                        explicitEnd = true;
                        return true;
                    }
                    return false;
                },
                parsedLine.statement);

            if (!emitted) {
                continue;
            }
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
