#include "BasicBytecodeEmitter.h"

#include "BasicBuiltinNames.h"
#include "BasicCompileUtils.h"

#include <functional>
#include <optional>
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

            bool emitBuiltinCall(const BasicAst::FunctionCallExpr &node);

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
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::FloatLiteralExpr>) {
                            out_.push_back(PickVM::Instruction{PickVM::OpCode::PushFlt, node.value});
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::StringLiteralExpr>) {
                            out_.push_back(PickVM::Instruction{PickVM::OpCode::PushStr, node.value});
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::DictFieldExpr>) {
                            // DICT field tokens resolve to an integer attribute index at runtime.
                            if (inArithmetic) {
                                error_ = "DICT field tokens cannot be used in an arithmetic expression";
                                return false;
                            }
                            if (node.token.empty()) {
                                error_ = "DICT field token cannot be empty";
                                return false;
                            }
                            out_.push_back(PickVM::Instruction{PickVM::OpCode::PushStr, node.token});
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
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::SubscriptExpr>) {
                            if (!node.indexExpr) {
                                error_ = "Array subscript requires an index expression";
                                return false;
                            }
                            if (!isValidVariableName(node.varName)) {
                                error_ = "Invalid array variable name '" + node.varName + "'";
                                return false;
                            }
                            if (inArithmetic && isStringVar(node.varName)) {
                                error_ = "String array '" + node.varName +
                                         "' cannot be used in an arithmetic expression";
                                return false;
                            }
                            // Emit index, then LOAD_ARR
                            if (!emitNode(*node.indexExpr, false)) {
                                return false;
                            }
                            out_.push_back(PickVM::Instruction{PickVM::OpCode::LoadArr, uppercase(node.varName)});
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::AttributeAccessExpr>) {
                            if (!node.attrExpr) {
                                error_ = "Attribute access requires an attribute expression";
                                return false;
                            }
                            if (!isValidVariableName(node.varName)) {
                                error_ = "Invalid variable name '" + node.varName + "'";
                                return false;
                            }
                            out_.push_back(PickVM::Instruction{PickVM::OpCode::LoadVar, uppercase(node.varName)});
                            if (!emitNode(*node.attrExpr, false)) {
                                return false;
                            }
                            if (node.valueIndexExpr) {
                                if (!emitNode(*node.valueIndexExpr, false)) {
                                    return false;
                                }
                            } else {
                                out_.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, 0});
                            }
                            emitNoOperand(PickVM::OpCode::ExtractAttr);
                            return true;
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::FunctionCallExpr>) {
                            (void) inArithmetic;
                            return emitBuiltinCall(node);
                        }
                        return false;
                    },
                    expression.node);
            }
        };

        bool ExpressionAstEmitter::emitBuiltinCall(const BasicAst::FunctionCallExpr &node) {
            const std::optional<int> expected = BasicBuiltins::arityForBuiltinCall(node.name);
            if (!expected.has_value()) {
                error_ = "Internal error: unsupported builtin " + node.name;
                return false;
            }
            if (static_cast<int>(node.arguments.size()) != *expected) {
                error_ = node.name + " expects " + std::to_string(*expected) + " argument(s), got " +
                         std::to_string(node.arguments.size());
                return false;
            }
            const bool argInArithmetic =
                (node.name == "ABS" || node.name == "SGN" || node.name == "SPACE");
            for (const auto &argPtr : node.arguments) {
                if (!argPtr) {
                    error_ = node.name + ": missing argument";
                    return false;
                }
                if (!emitNode(*argPtr, argInArithmetic)) {
                    return false;
                }
            }
            out_.push_back(PickVM::Instruction{PickVM::OpCode::InvokeBuiltin, node.name});
            return true;
        }

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

        std::function<bool(const BasicIr::NormalizedStmt &, int)> emitInlineStatement;
        std::function<bool(const BasicIr::BranchArm &, int)> emitBranchArm;

        emitInlineStatement = [&](const BasicIr::NormalizedStmt &stmtNode, const int sourceLine) -> bool {
            return std::visit(
                [&](const auto &stmt) -> bool {
                    using StmtT = std::decay_t<decltype(stmt)>;
                    if constexpr (std::is_same_v<StmtT, BasicIr::LetStmt>) {
                        if (!stmt.expression) {
                            result.errors.push_back({sourceLine, "LET requires an expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.expression)) {
                            result.errors.push_back({sourceLine, "LET expression error: " + error});
                            return false;
                        }
                        if (isIntVar(stmt.variableName)) {
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::CoerceInt));
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, uppercase(stmt.variableName)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::InputStmt>) {
                        if (stmt.promptExpr) {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.promptExpr)) {
                                result.errors.push_back({sourceLine, "INPUT prompt expression error: " + error});
                                return false;
                            }
                            // Prompted INPUT emits PRINT without newline, then INPUT.
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintVal));
                        }
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::InputStr));
                        if (isIntVar(stmt.variableName)) {
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::CoerceInt));
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, uppercase(stmt.variableName)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ChainStmt>) {
                        if (!stmt.programExpr) {
                            result.errors.push_back({sourceLine, "CHAIN requires a program expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.programExpr)) {
                            result.errors.push_back({sourceLine, "CHAIN program expression error: " + error});
                            return false;
                        }
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Chain));
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::GotoStmt>) {
                        const std::size_t jumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        jumpFixups.push_back({sourceLine, jumpIp, stmt.targetLine});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::GosubStmt>) {
                        const std::size_t callIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Call, 0});
                        jumpFixups.push_back({sourceLine, callIp, stmt.targetLine});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ReturnStmt>) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Return));
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::IfStmt>) {
                        if (!stmt.condition) {
                            result.errors.push_back({sourceLine, "IF requires a condition expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.condition)) {
                            result.errors.push_back({sourceLine, "IF condition error: " + error});
                            return false;
                        }
                        const std::size_t jzIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                        if (!emitBranchArm(stmt.thenArm, sourceLine)) {
                            return false;
                        }
                        if (stmt.elseArm.has_value()) {
                            const bool thenFallsThrough = !stmt.thenArm.line.has_value();
                            std::size_t jumpAfterElseIp = 0;
                            if (thenFallsThrough) {
                                jumpAfterElseIp = result.program.size();
                                result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            }
                            const std::size_t elseStartIp = result.program.size();
                            if (!emitBranchArm(*stmt.elseArm, sourceLine)) {
                                return false;
                            }
                            result.program[jzIp].operand = static_cast<int>(elseStartIp);
                            if (thenFallsThrough) {
                                result.program[jumpAfterElseIp].operand = static_cast<int>(result.program.size());
                            }
                        } else {
                            result.program[jzIp].operand = static_cast<int>(result.program.size());
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::PrintStmt>) {
                        if (!stmt.expression) {
                            result.errors.push_back({sourceLine, "PRINT requires an expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.expression)) {
                            result.errors.push_back({sourceLine, "PRINT expression error: " + error});
                            return false;
                        }
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintVal));
                        if (!stmt.suppressEol) {
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintEol));
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::StopStmt> ||
                                         std::is_same_v<StmtT, BasicIr::EndStmt>) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::OpenStmt>) {
                        if (!stmt.fileExpr) {
                            result.errors.push_back({sourceLine, "OPEN requires a file expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.fileExpr)) {
                            result.errors.push_back({sourceLine, "OPEN file expression error: " + error});
                            return false;
                        }
                        const std::string fileVar = uppercase(stmt.fileVar);
                        if (!stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::OpenFile, fileVar});
                            return true;
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::OpenFileTry, fileVar});
                        const std::size_t jzIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                        const std::size_t skipElseJumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        const std::size_t failPathIp = result.program.size();
                        if (!emitBranchArm(*stmt.elseArm, sourceLine)) {
                            return false;
                        }
                        result.program[jzIp].operand = static_cast<int>(failPathIp);
                        result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ReadStmt>) {
                        if (!stmt.idExpr) {
                            result.errors.push_back({sourceLine, "READ requires an ID expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.idExpr)) {
                            result.errors.push_back({sourceLine, "READ ID expression error: " + error});
                            return false;
                        }
                        const std::string fileVar = uppercase(stmt.fileVar);
                        const std::string targetVar = uppercase(stmt.targetVar);
                        if (!stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadRec, fileVar});
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                            return true;
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadRecTry, fileVar});
                        const std::size_t jzIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                        const std::size_t skipElseJumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        const std::size_t failPathIp = result.program.size();
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Drop));
                        if (!emitBranchArm(*stmt.elseArm, sourceLine)) {
                            return false;
                        }
                        result.program[jzIp].operand = static_cast<int>(failPathIp);
                        result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::WriteStmt>) {
                        if (!stmt.valueExpr || !stmt.idExpr) {
                            result.errors.push_back({sourceLine, "WRITE requires value and ID expressions"});
                            return false;
                        }
                        {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.valueExpr)) {
                                result.errors.push_back({sourceLine, "WRITE value expression error: " + error});
                                return false;
                            }
                        }
                        {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.idExpr)) {
                                result.errors.push_back({sourceLine, "WRITE ID expression error: " + error});
                                return false;
                            }
                        }
                        const std::string fileVar = uppercase(stmt.fileVar);
                        if (!stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::WriteRec, fileVar});
                            return true;
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::WriteRecTry, fileVar});
                        const std::size_t jzIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                        const std::size_t skipElseJumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        const std::size_t failPathIp = result.program.size();
                        if (!emitBranchArm(*stmt.elseArm, sourceLine)) {
                            return false;
                        }
                        result.program[jzIp].operand = static_cast<int>(failPathIp);
                        result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ReadNextStmt>) {
                        const std::string fileVar = uppercase(stmt.fileVar);
                        const std::string targetVar = uppercase(stmt.targetVar);
                        if (!stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadNext, fileVar});
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                            return true;
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadNextTry, fileVar});
                        const std::size_t jzIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                        const std::size_t skipElseJumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        const std::size_t failPathIp = result.program.size();
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Drop));
                        if (!emitBranchArm(*stmt.elseArm, sourceLine)) {
                            return false;
                        }
                        result.program[jzIp].operand = static_cast<int>(failPathIp);
                        result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ReadVStmt>) {
                        if (!stmt.idExpr || !stmt.attrExpr) {
                            result.errors.push_back({sourceLine, "READV requires ID and attribute expressions"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.idExpr)) {
                            result.errors.push_back({sourceLine, "READV ID expression error: " + error});
                            return false;
                        }
                        if (!emitter.emit(*stmt.attrExpr)) {
                            result.errors.push_back({sourceLine, "READV attribute expression error: " + error});
                            return false;
                        }
                        if (stmt.valueIndexExpr) {
                            if (!emitter.emit(*stmt.valueIndexExpr)) {
                                result.errors.push_back({sourceLine, "READV value-index expression error: " + error});
                                return false;
                            }
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, 0});
                        }
                        const std::string fileVar = uppercase(stmt.fileVar);
                        const std::string targetVar = uppercase(stmt.targetVar);
                        if (!stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadV, fileVar});
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                            return true;
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadVTry, fileVar});
                        const std::size_t jzIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                        const std::size_t skipElseJumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        const std::size_t failPathIp = result.program.size();
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Drop));
                        if (!emitBranchArm(*stmt.elseArm, sourceLine)) {
                            return false;
                        }
                        result.program[jzIp].operand = static_cast<int>(failPathIp);
                        result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::WriteVStmt>) {
                        if (!stmt.valueExpr || !stmt.idExpr || !stmt.attrExpr) {
                            result.errors.push_back({sourceLine, "WRITEV requires value, ID, and attribute expressions"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.valueExpr)) {
                            result.errors.push_back({sourceLine, "WRITEV value expression error: " + error});
                            return false;
                        }
                        if (!emitter.emit(*stmt.idExpr)) {
                            result.errors.push_back({sourceLine, "WRITEV ID expression error: " + error});
                            return false;
                        }
                        if (!emitter.emit(*stmt.attrExpr)) {
                            result.errors.push_back({sourceLine, "WRITEV attribute expression error: " + error});
                            return false;
                        }
                        if (stmt.valueIndexExpr) {
                            if (!emitter.emit(*stmt.valueIndexExpr)) {
                                result.errors.push_back({sourceLine, "WRITEV value-index expression error: " + error});
                                return false;
                            }
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, 0});
                        }
                        const std::string fileVar = uppercase(stmt.fileVar);
                        if (!stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::WriteV, fileVar});
                            return true;
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::WriteVTry, fileVar});
                        const std::size_t jzIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                        const std::size_t skipElseJumpIp = result.program.size();
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                        const std::size_t failPathIp = result.program.size();
                        if (!emitBranchArm(*stmt.elseArm, sourceLine)) {
                            return false;
                        }
                        result.program[jzIp].operand = static_cast<int>(failPathIp);
                        result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::CloseStmt>) {
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::CloseFile, uppercase(stmt.fileVar)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ClearStmt>) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::ClearVars));
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::RemStmt>) {
                        return true;
                    } else {
                        result.errors.push_back({sourceLine, "Unsupported inline branch statement"});
                        return false;
                    }
                },
                stmtNode);
        };

        emitBranchArm = [&](const BasicIr::BranchArm &arm, const int sourceLine) -> bool {
            if (arm.line.has_value()) {
                const std::size_t jumpIp = result.program.size();
                result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                jumpFixups.push_back({sourceLine, jumpIp, *arm.line});
                return true;
            }

            if (!arm.inlineStatement) {
                result.errors.push_back({sourceLine, "Branch arm requires a line target or statement"});
                return false;
            }
            return emitInlineStatement(arm.inlineStatement->statement, sourceLine);
        };

        for (const BasicIr::NormalizedLine &line: program.lines) {
            const std::size_t lineStartIp = result.program.size();
            lineToInstructionIndex[line.lineNumber] = lineStartIp;
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
                        if (stmt.promptExpr) {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.promptExpr)) {
                                result.errors.push_back({line.lineNumber, "INPUT prompt expression error: " + error});
                                return false;
                            }
                            // Prompted INPUT emits PRINT without newline, then INPUT.
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintVal));
                        }
                        // All INPUT reads a raw string line; % variables additionally coerce to int.
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::InputStr));
                        if (isIntVar(stmt.variableName)) {
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::CoerceInt));
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, uppercase(stmt.variableName)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ChainStmt>) {
                        if (!stmt.programExpr) {
                            result.errors.push_back({line.lineNumber, "CHAIN requires a program expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.programExpr)) {
                            result.errors.push_back({line.lineNumber, "CHAIN program expression error: " + error});
                            return false;
                        }
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Chain));
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
                        if (!emitBranchArm(stmt.thenArm, line.lineNumber)) {
                            return false;
                        }
                        if (stmt.elseArm.has_value()) {
                            const bool thenFallsThrough = !stmt.thenArm.line.has_value();
                            std::size_t jumpAfterElseIp = 0;
                            if (thenFallsThrough) {
                                jumpAfterElseIp = result.program.size();
                                result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            }
                            const std::size_t elseStartIp = result.program.size();
                            if (!emitBranchArm(*stmt.elseArm, line.lineNumber)) {
                                return false;
                            }
                            result.program[jzIp].operand = static_cast<int>(elseStartIp);
                            if (thenFallsThrough) {
                                result.program[jumpAfterElseIp].operand = static_cast<int>(result.program.size());
                            }
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
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::DimStmt>) {
                        if (!stmt.sizeExpr) {
                            result.errors.push_back({line.lineNumber, "DIM requires a size expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.sizeExpr)) {
                            result.errors.push_back({line.lineNumber, "DIM size error: " + error});
                            return false;
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::DimArray, uppercase(stmt.variableName)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::LetArrayStmt>) {
                        if (!stmt.indexExpr || !stmt.valueExpr) {
                            result.errors.push_back({line.lineNumber, "LET array requires index and value expressions"});
                            return false;
                        }
                        // Emit value expression
                        {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.valueExpr)) {
                                result.errors.push_back({line.lineNumber, "LET array value error: " + error});
                                return false;
                            }
                        }
                        // % suffix → coerce value to int
                        if (isIntVar(stmt.variableName)) {
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::CoerceInt));
                        }
                        // Emit index expression
                        {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.indexExpr)) {
                                result.errors.push_back({line.lineNumber, "LET array index error: " + error});
                                return false;
                            }
                        }
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreArr, uppercase(stmt.variableName)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::OpenStmt>) {
                        if (!stmt.fileExpr) {
                            result.errors.push_back({line.lineNumber, "OPEN requires a file expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.fileExpr)) {
                            result.errors.push_back({line.lineNumber, "OPEN file expression error: " + error});
                            return false;
                        }

                        const std::string fileVar = uppercase(stmt.fileVar);
                        if (stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::OpenFileTry, fileVar});
                            const std::size_t jzIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                            const std::size_t skipElseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            const std::size_t elseStartIp = result.program.size();
                            if (!emitBranchArm(*stmt.elseArm, line.lineNumber)) {
                                return false;
                            }
                            result.program[jzIp].operand = static_cast<int>(elseStartIp);
                            result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::OpenFile, fileVar});
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ReadStmt>) {
                        if (!stmt.idExpr) {
                            result.errors.push_back({line.lineNumber, "READ requires an ID expression"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        if (!emitter.emit(*stmt.idExpr)) {
                            result.errors.push_back({line.lineNumber, "READ ID expression error: " + error});
                            return false;
                        }

                        const std::string fileVar = uppercase(stmt.fileVar);
                        const std::string targetVar = uppercase(stmt.targetVar);
                        if (stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadRecTry, fileVar});
                            const std::size_t jzIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0}); // pops success flag
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::StoreVar));
                            result.program.back().operand = targetVar;
                            const std::size_t skipElseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            const std::size_t failPathIp = result.program.size();
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Drop)); // drop placeholder value
                            result.program[jzIp].operand = static_cast<int>(failPathIp);
                            if (!emitBranchArm(*stmt.elseArm, line.lineNumber)) {
                                return false;
                            }
                            result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadRec, fileVar});
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::WriteStmt>) {
                        if (!stmt.valueExpr || !stmt.idExpr) {
                            result.errors.push_back({line.lineNumber, "WRITE requires value and ID expressions"});
                            return false;
                        }
                        {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.valueExpr)) {
                                result.errors.push_back({line.lineNumber, "WRITE value expression error: " + error});
                                return false;
                            }
                        }
                        {
                            std::string error;
                            ExpressionAstEmitter emitter(result.program, error);
                            if (!emitter.emit(*stmt.idExpr)) {
                                result.errors.push_back({line.lineNumber, "WRITE ID expression error: " + error});
                                return false;
                            }
                        }

                        const std::string fileVar = uppercase(stmt.fileVar);
                        if (stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::WriteRecTry, fileVar});
                            const std::size_t jzIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                            const std::size_t skipElseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            const std::size_t elseStartIp = result.program.size();
                            if (!emitBranchArm(*stmt.elseArm, line.lineNumber)) {
                                return false;
                            }
                            result.program[jzIp].operand = static_cast<int>(elseStartIp);
                            result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::WriteRec, fileVar});
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ReadNextStmt>) {
                        const std::string fileVar = uppercase(stmt.fileVar);
                        const std::string targetVar = uppercase(stmt.targetVar);
                        if (stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadNextTry, fileVar});
                            const std::size_t jzIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                            const std::size_t skipElseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            const std::size_t failPathIp = result.program.size();
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Drop));
                            if (!emitBranchArm(*stmt.elseArm, line.lineNumber)) {
                                return false;
                            }
                            result.program[jzIp].operand = static_cast<int>(failPathIp);
                            result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadNext, fileVar});
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ReadVStmt>) {
                        if (!stmt.idExpr || !stmt.attrExpr) {
                            result.errors.push_back({line.lineNumber, "READV requires ID and attribute expressions"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        const std::string fileVar = uppercase(stmt.fileVar);
                        if (!emitter.emit(*stmt.idExpr)) {
                            result.errors.push_back({line.lineNumber, "READV ID expression error: " + error});
                            return false;
                        }
                        if (!emitter.emit(*stmt.attrExpr)) {
                            result.errors.push_back({line.lineNumber, "READV attribute expression error: " + error});
                            return false;
                        }
                        // DICT<token> in READV/WRITEV attribute position is resolved to an integer
                        // attribute number just before the READV/WRITEV runtime opcodes.
                        if (std::holds_alternative<BasicAst::DictFieldExpr>(stmt.attrExpr->node)) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ResolveDictAttr, fileVar});
                        }
                        if (stmt.valueIndexExpr) {
                            if (!emitter.emit(*stmt.valueIndexExpr)) {
                                result.errors.push_back({line.lineNumber, "READV value-index expression error: " + error});
                                return false;
                            }
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, 0});
                        }
                        const std::string targetVar = uppercase(stmt.targetVar);
                        if (stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadVTry, fileVar});
                            const std::size_t jzIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                            const std::size_t skipElseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            const std::size_t failPathIp = result.program.size();
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Drop));
                            if (!emitBranchArm(*stmt.elseArm, line.lineNumber)) {
                                return false;
                            }
                            result.program[jzIp].operand = static_cast<int>(failPathIp);
                            result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadV, fileVar});
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, targetVar});
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::WriteVStmt>) {
                        if (!stmt.valueExpr || !stmt.idExpr || !stmt.attrExpr) {
                            result.errors.push_back({line.lineNumber, "WRITEV requires value, ID, and attribute expressions"});
                            return false;
                        }
                        std::string error;
                        ExpressionAstEmitter emitter(result.program, error);
                        const std::string fileVar = uppercase(stmt.fileVar);
                        if (!emitter.emit(*stmt.valueExpr)) {
                            result.errors.push_back({line.lineNumber, "WRITEV value expression error: " + error});
                            return false;
                        }
                        if (!emitter.emit(*stmt.idExpr)) {
                            result.errors.push_back({line.lineNumber, "WRITEV ID expression error: " + error});
                            return false;
                        }
                        if (!emitter.emit(*stmt.attrExpr)) {
                            result.errors.push_back({line.lineNumber, "WRITEV attribute expression error: " + error});
                            return false;
                        }
                        if (std::holds_alternative<BasicAst::DictFieldExpr>(stmt.attrExpr->node)) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ResolveDictAttr, fileVar});
                        }
                        if (stmt.valueIndexExpr) {
                            if (!emitter.emit(*stmt.valueIndexExpr)) {
                                result.errors.push_back({line.lineNumber, "WRITEV value-index expression error: " + error});
                                return false;
                            }
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, 0});
                        }
                        if (stmt.elseArm.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::WriteVTry, fileVar});
                            const std::size_t jzIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                            const std::size_t skipElseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            const std::size_t failPathIp = result.program.size();
                            if (!emitBranchArm(*stmt.elseArm, line.lineNumber)) {
                                return false;
                            }
                            result.program[jzIp].operand = static_cast<int>(failPathIp);
                            result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::WriteV, fileVar});
                        }
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::CloseStmt>) {
                        result.program.push_back(PickVM::Instruction{PickVM::OpCode::CloseFile, uppercase(stmt.fileVar)});
                        return true;
                    } else if constexpr (std::is_same_v<StmtT, BasicIr::ClearStmt>) {
                        result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::ClearVars));
                        return true;
                    }
                    return false;
                },
                line.statement);

            if (!emitted) {
                continue;
            }
            const std::size_t lineEndIp = result.program.size();
            if (lineEndIp > lineStartIp) {
                result.sourceLinePerInstr.insert(
                    result.sourceLinePerInstr.end(),
                    lineEndIp - lineStartIp,
                    line.lineNumber);
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
            result.sourceLinePerInstr.clear();
            result.success = false;
            return result;
        }

        if (!explicitEnd || result.program.empty() || result.program.back().op != PickVM::OpCode::Halt) {
            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
            result.sourceLinePerInstr.push_back(0); // implicit HALT has no source line
        }

        result.success = true;
        return result;
    }
} // namespace PickShell
