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
                        } else if constexpr (std::is_same_v<NodeT, BasicAst::FunctionCallExpr>) {
                            if (!node.argument) {
                                error_ = node.name + ": missing argument";
                                return false;
                            }
                            const bool argInArithmetic = (node.name == "ABS" || node.name == "SGN");
                            if (!emitNode(*node.argument, argInArithmetic)) {
                                return false;
                            }
                            if (node.name == "ABS") {
                                emitNoOperand(PickVM::OpCode::AbsInt);
                            } else if (node.name == "SGN") {
                                emitNoOperand(PickVM::OpCode::SgnInt);
                            } else {
                                emitNoOperand(PickVM::OpCode::SeqStr);
                            }
                            return true;
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
                        if (!stmt.thenArm.line.has_value()) {
                            result.errors.push_back({line.lineNumber, "IF THEN requires a line target"});
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
                        jumpFixups.push_back({line.lineNumber, thenJumpIp, *stmt.thenArm.line});
                        if (stmt.elseArm.has_value() && stmt.elseArm->line.has_value()) {
                            result.program[jzIp].operand = static_cast<int>(result.program.size());
                            const std::size_t elseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            jumpFixups.push_back({line.lineNumber, elseJumpIp, *stmt.elseArm->line});
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
                        if (stmt.elseArm.has_value() && stmt.elseArm->line.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::OpenFileTry, fileVar});
                            const std::size_t jzIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                            const std::size_t skipElseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            const std::size_t elseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            result.program[jzIp].operand = static_cast<int>(elseJumpIp);
                            result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                            jumpFixups.push_back({line.lineNumber, elseJumpIp, *stmt.elseArm->line});
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
                        if (stmt.elseArm.has_value() && stmt.elseArm->line.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::ReadRecTry, fileVar});
                            const std::size_t jzIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0}); // pops success flag
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::StoreVar));
                            result.program.back().operand = targetVar;
                            const std::size_t skipElseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            const std::size_t failPathIp = result.program.size();
                            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Drop)); // drop placeholder value
                            const std::size_t elseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            result.program[jzIp].operand = static_cast<int>(failPathIp);
                            result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                            jumpFixups.push_back({line.lineNumber, elseJumpIp, *stmt.elseArm->line});
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
                        if (stmt.elseArm.has_value() && stmt.elseArm->line.has_value()) {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::WriteRecTry, fileVar});
                            const std::size_t jzIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::JumpIfZero, 0});
                            const std::size_t skipElseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            const std::size_t elseJumpIp = result.program.size();
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::Jump, 0});
                            result.program[jzIp].operand = static_cast<int>(elseJumpIp);
                            result.program[skipElseJumpIp].operand = static_cast<int>(result.program.size());
                            jumpFixups.push_back({line.lineNumber, elseJumpIp, *stmt.elseArm->line});
                        } else {
                            result.program.push_back(PickVM::Instruction{PickVM::OpCode::WriteRec, fileVar});
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
