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
            case OpCode::PushStr: return "PUSH_STR";
            case OpCode::Add: return "ADD";
            case OpCode::Sub: return "SUB";
            case OpCode::Concat: return "CONCAT";
            case OpCode::Dup: return "DUP";
            case OpCode::Drop: return "DROP";
            case OpCode::PrintInt: return "PRINT_INT";
            case OpCode::PrintStr: return "PRINT_STR";
            case OpCode::Jump: return "JUMP";
            case OpCode::JumpIfZero: return "JZ";
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
            case OpCode::PushStr: {
                const std::string s = stringOperandAtIp(instr, ip);
                oss << " \"" << s << '"';
                break;
            }
            case OpCode::Jump:
            case OpCode::JumpIfZero:
                oss << ' ' << intOperandAtIp(instr, ip);
                break;
            case OpCode::Add:
            case OpCode::Sub:
            case OpCode::Concat:
            case OpCode::Dup:
            case OpCode::Drop:
            case OpCode::PrintInt:
            case OpCode::PrintStr:
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
