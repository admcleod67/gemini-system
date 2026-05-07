//
// Created by Allan McLeod on 18/04/2026.
//

#include "InstructionPrint.h"
#include "Parser.h"

#include <sstream>
#include <stdexcept>
#include <variant>

namespace PickVM {
    const char *opCodeName(OpCode op) {
        switch (op) {
            case OpCode::Halt: return "HALT";
            case OpCode::PushInt: return "PUSH_INT";
            case OpCode::PushFlt: return "PUSH_FLT";
            case OpCode::PushStr: return "PUSH_STR";
            case OpCode::Add: return "ADD";
            case OpCode::Sub: return "SUB";
            case OpCode::Mul: return "MUL";
            case OpCode::Div: return "DIV";
            case OpCode::Eq: return "EQ";
            case OpCode::Ne: return "NE";
            case OpCode::Lt: return "LT";
            case OpCode::Le: return "LE";
            case OpCode::Gt: return "GT";
            case OpCode::Ge: return "GE";
            case OpCode::Concat: return "CONCAT";
            case OpCode::Dup: return "DUP";
            case OpCode::Drop: return "DROP";
            case OpCode::PrintInt: return "PRINT_INT";
            case OpCode::PrintStr: return "PRINT_STR";
            case OpCode::PrintVal: return "PRINT_VAL";
            case OpCode::PrintEol: return "PRINT_EOL";
            case OpCode::InputInt: return "INPUT_INT";
            case OpCode::InputStr: return "INPUT_STR";
            case OpCode::CoerceInt: return "COERCE_INT";
            case OpCode::Jump: return "JUMP";
            case OpCode::JumpIfZero: return "JZ";
            case OpCode::Call: return "CALL";
            case OpCode::Return: return "RETURN";
            case OpCode::ForSetup: return "FOR_SETUP";
            case OpCode::ForNext: return "FOR_NEXT";
            case OpCode::DimArray: return "DIM_ARRAY";
            case OpCode::LoadArr: return "LOAD_ARR";
            case OpCode::StoreArr: return "STORE_ARR";
            case OpCode::ClearVars: return "CLEAR_VARS";
            case OpCode::AbsInt: return "ABS_INT";
            case OpCode::SgnInt: return "SGN_INT";
            case OpCode::SeqStr: return "SEQ_STR";
            case OpCode::OpenFile: return "OPEN_FILE";
            case OpCode::OpenFileTry: return "OPEN_FILE_TRY";
            case OpCode::ReadRec: return "READ_REC";
            case OpCode::ReadRecTry: return "READ_REC_TRY";
            case OpCode::WriteRec: return "WRITE_REC";
            case OpCode::WriteRecTry: return "WRITE_REC_TRY";
            case OpCode::ReadNext: return "READ_NEXT";
            case OpCode::ReadNextTry: return "READ_NEXT_TRY";
            case OpCode::ReadV: return "READ_V";
            case OpCode::ReadVTry: return "READ_V_TRY";
            case OpCode::WriteV: return "WRITE_V";
            case OpCode::WriteVTry: return "WRITE_V_TRY";
            case OpCode::CloseFile: return "CLOSE_FILE";
            case OpCode::LoadVar: return "LOAD_VAR";
            case OpCode::StoreVar: return "STORE_VAR";
        }
        return "UNKNOWN";
    }

    namespace {
        int intOperandAtIp(const Instruction &instr, std::size_t ip) {
            if (!std::holds_alternative<int>(instr.operand)) {
                std::ostringstream oss;
                oss << "Instruction " << ip << " (" << opCodeName(instr.op) << "): expected int operand";
                throw std::runtime_error(oss.str());
            }
            return std::get<int>(instr.operand);
        }

        std::string stringOperandAtIp(const Instruction &instr, std::size_t ip) {
            if (!std::holds_alternative<std::string>(instr.operand)) {
                std::ostringstream oss;
                oss << "Instruction " << ip << " (" << opCodeName(instr.op) << "): expected string operand";
                throw std::runtime_error(oss.str());
            }
            return std::get<std::string>(instr.operand);
        }

        double floatOperandAtIp(const Instruction &instr, std::size_t ip) {
            if (!std::holds_alternative<double>(instr.operand)) {
                std::ostringstream oss;
                oss << "Instruction " << ip << " (" << opCodeName(instr.op) << "): expected float operand";
                throw std::runtime_error(oss.str());
            }
            return std::get<double>(instr.operand);
        }
    } // namespace

    std::string formatInstructionLine(std::size_t ip, const Instruction &instr, const LoadedBytecode *loaded) {
        std::ostringstream oss;
        oss << ip << ": " << opCodeName(instr.op);

        switch (instr.op) {
            case OpCode::Halt:
                break;
            case OpCode::PushInt:
                oss << ' ' << intOperandAtIp(instr, ip);
                break;
            case OpCode::PushFlt:
                oss << ' ' << floatOperandAtIp(instr, ip);
                break;
            case OpCode::PushStr: {
                const std::string s = stringOperandAtIp(instr, ip);
                oss << " \"" << s << '"';
                break;
            }
            case OpCode::Jump:
            case OpCode::JumpIfZero:
            case OpCode::Call:
                oss << ' ' << intOperandAtIp(instr, ip);
                break;
            case OpCode::LoadVar:
            case OpCode::StoreVar:
            case OpCode::ForSetup:
            case OpCode::ForNext:
            case OpCode::DimArray:
            case OpCode::LoadArr:
            case OpCode::StoreArr:
            case OpCode::OpenFile:
            case OpCode::OpenFileTry:
            case OpCode::ReadRec:
            case OpCode::ReadRecTry:
            case OpCode::WriteRec:
            case OpCode::WriteRecTry:
            case OpCode::ReadNext:
            case OpCode::ReadNextTry:
            case OpCode::ReadV:
            case OpCode::ReadVTry:
            case OpCode::WriteV:
            case OpCode::WriteVTry:
            case OpCode::CloseFile:
                oss << ' ' << stringOperandAtIp(instr, ip);
                break;
            case OpCode::Add:
            case OpCode::Sub:
            case OpCode::Mul:
            case OpCode::Div:
            case OpCode::Eq:
            case OpCode::Ne:
            case OpCode::Lt:
            case OpCode::Le:
            case OpCode::Gt:
            case OpCode::Ge:
            case OpCode::Concat:
            case OpCode::Dup:
            case OpCode::Drop:
            case OpCode::PrintInt:
            case OpCode::PrintStr:
            case OpCode::PrintVal:
            case OpCode::PrintEol:
            case OpCode::InputInt:
            case OpCode::InputStr:
            case OpCode::CoerceInt:
            case OpCode::Return:
            case OpCode::ClearVars:
            case OpCode::AbsInt:
            case OpCode::SgnInt:
            case OpCode::SeqStr:
                break;
        }

        if (loaded && ip < loaded->sourceLinePerInstr.size()) {
            const int line = loaded->sourceLinePerInstr[ip];
            if (line > 0) {
                oss << " (line " << line << ')';
            }
        }

        return oss.str();
    }
} // namespace PickVM
