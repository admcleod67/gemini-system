#include "BasicBytecodeEmitter.h"

#include "BasicCompileUtils.h"

#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace PickShell {
    namespace {
        PickVM::Instruction makeNoOperandInstruction(const PickVM::OpCode op) {
            return PickVM::Instruction{op, PickVM::Value{}};
        }

        // Variable name suffix helpers (Pick BASIC conventions).
        // $ → force string; % → force integer; no suffix → dynamic.
        bool isStringVar(const std::string &name) {
            return !name.empty() && name.back() == '$';
        }

        bool isIntVar(const std::string &name) {
            return !name.empty() && name.back() == '%';
        }

        // Returns true when a BinaryOp is an arithmetic operator (not a comparison).
        bool isArithmeticOp(const BasicAst::BinaryOp op) {
            return op == BasicAst::BinaryOp::Add ||
                   op == BasicAst::BinaryOp::Subtract ||
                   op == BasicAst::BinaryOp::Multiply ||
                   op == BasicAst::BinaryOp::Divide;
        }

        class ExpressionAstEmitter {
        public:
            ExpressionAstEmitter(std::vector<PickVM::Instruction> &out, std::string &error)
                : out_(out), error_(error) {
            }

            [[nodiscard]] bool emit(const BasicAst::Expr &expression) {
                if (!emitNode(expression, false)) {
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

            // inArithmetic is true when this node is an operand of an arithmetic operator
            // or unary negate — used to reject $ variables in numeric contexts.
            bool emitNode(const BasicAst::Expr &expression, bool inArithmetic) {
                return std::visit(
                    [this, inArithmetic](const auto &node) -> bool {
                        using NodeT = std::decay_t<decltype(node)>;
                        if constexpr (std::is_same_v<NodeT, BasicAst::IntLiteralExpr>) {
                            out_.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, node.value});
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::StringLiteralExpr>) {
                            out_.push_back(PickVM::Instruction{PickVM::OpCode::PushStr, node.value});
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::IdentifierExpr>) {
                            if (!isValidVariableName(node.name)) {
                                error_ = "Invalid variable name '" + node.name + "'";
                                return false;
                            }
                            // $ variables may never be used in arithmetic contexts.
                            if (inArithmetic && isStringVar(node.name)) {
                                error_ = "String variable '" + node.name +
                                         "' cannot be used in an arithmetic expression";
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
                            if (!emitNode(*node.operand, true)) {
                                return false;
                            }
                            emitNoOperand(PickVM::OpCode::Sub);
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::BinaryExpr>) {
                            if (!node.left || !node.right) {
                                error_ = "Invalid binary expression";
                                return false;
                            }
                            const bool childInArithmetic = isArithmeticOp(node.op);
                            if (!emitNode(*node.left, childInArithmetic) ||
                                !emitNode(*node.right, childInArithmetic)) {
                                return false;
                            }
                            emitNoOperand(toOpCode(node.op));
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::GroupedExpr>) {
                            if (!node.expression) {
                                error_ = "Invalid grouped expression";
                                return false;
                            }
                            return emitNode(*node.expression, inArithmetic);
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

    BasicBytecodeEmissionResult BasicBytecodeEmitter::emit(const BasicIr::NormalizedProgram &program) {
        BasicBytecodeEmissionResult result;
        bool explicitEnd = false;
        std::unordered_map<int, std::size_t> lineToInstructionIndex;
        std::vector<JumpFixup> jumpFixups;

        for (const BasicIr::NormalizedLine &line: program.lines) {
            lineToInstructionIndex[line.lineNumber] = result.program.size();
            const bool emitted = std::visit(
                [&](const auto &stmt) -> bool {
                    using StmtT = std::decay_t<decltype(stmt)>;
                    if constexpr (std::is_same_v<StmtT, BasicIr::LetStmt>) {
                        if (!stmt.expression) {
                            result.errors.push_back({line.lineNumber, "LET requires an expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.expression)) {
                            result.errors.push_back({line.lineNumber, "LET expression error: " + error});
                            return false;
                        }
                        // % variables are always integers: coerce on assignment.
                        if (isIntVar(stmt.variableName)) {
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::CoerceInt));
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, uppercase(stmt.variableName)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::InputStmt>) {
                        // All INPUT reads a raw string line; % variables additionally coerce to int.
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::InputStr));
                        if (isIntVar(stmt.variableName)) {
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::CoerceInt));
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, uppercase(stmt.variableName)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::GotoStmt>) {
                        const std::size_t jumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        jumpFixups.push_back({line.lineNumber, jumpIp, stmt.targetLine});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::GosubStmt>) {
                        const std::size_t callIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Call, 0});
                        jumpFixups.push_back({line.lineNumber, callIp, stmt.targetLine});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ReturnStmt>) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Return));
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ForStmt>) {
                        if (isStringVar(stmt.variableName)) {
                            result.errors.push_back({line.lineNumber,
                                "String variable '" + stmt.variableName +
                                "' cannot be used as a FOR loop variable"});
                            return false;
                        }
                        if (!stmt.initExpr || !stmt.limitExpr) {
                            result.errors.push_back({line.lineNumber, "FOR requires init and limit expressions"});
                            return false;
                        }
                        // Emit: init → [CoerceInt if %] → StoreVar
                        {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.initExpr)) {
                                result.errors.push_back({line.lineNumber, "FOR init error: " + error});
                                return false;
                            }
                        }
                        if (isIntVar(stmt.variableName)) {
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::CoerceInt));
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, uppercase(stmt.variableName)});
                        // Emit: limit expression
                        {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.limitExpr)) {
                                result.errors.push_back({line.lineNumber, "FOR limit error: " + error});
                                return false;
                            }
                        }
                        // Emit: step expression (or PUSH_INT 1 as default)
                        if (stmt.stepExpr) {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.stepExpr)) {
                                result.errors.push_back({line.lineNumber, "FOR STEP error: " + error});
                                return false;
                            }
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, 1});
                        }
                        // Emit: FOR_SETUP varName
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::ForSetup, uppercase(stmt.variableName)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::NextStmt>) {
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::ForNext, uppercase(stmt.variableName)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::IfStmt>) {
                        if (!stmt.condition) {
                            result.errors.push_back({line.lineNumber, "IF requires a condition expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.condition)) {
                            result.errors.push_back({line.lineNumber, "IF condition error: " + error});
                            return false;
                        }
                        const std::size_t jzIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                        const std::size_t thenJumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        jumpFixups.push_back({line.lineNumber, thenJumpIp, stmt.thenLine});
                        if (stmt.elseLine.has_value()) {
                            result.program[jzIp].operand = static_cast<int>(result.program.size());
                            const std::size_t elseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            jumpFixups.push_back({line.lineNumber, elseJumpIp, *stmt.elseLine});
                        } else {
                            result.program[jzIp].operand = static_cast<int>(result.program.size());
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::PrintStmt>) {
                        if (!stmt.expression) {
                            result.errors.push_back({line.lineNumber, "PRINT requires an expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.expression)) {
                            result.errors.push_back({line.lineNumber, "PRINT expression error: " + error});
                            return false;
                        }
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintVal));
                        if (!stmt.suppressEol) {
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintEol));
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::RemStmt>) {
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::StopStmt>) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::EndStmt>) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
                        explicitEnd = true;
                        return true;
                    }
                    return false;
                },
                line.statement);

            if (!emitted) {
                continue;
            }
        }

        for (const JumpFixup &fixup: jumpFixups) {
            const auto it = lineToInstructionIndex.find(fixup.targetLine);
            if (it == lineToInstructionIndex.end()) {
                continue;
            }
            result.program[fixup.instructionIndex].operand = static_cast<int>(it->second);
        }

        if (!result.errors.empty()) {
            result.program.clear();
            result.success = false;
            return result;
        }

        if (!explicitEnd || result.program.empty() || result.program.back().op != PickVM::OpCode::Halt) {
            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
        }

        result.success = true;
        return result;
    }
} // namespace PickShell
