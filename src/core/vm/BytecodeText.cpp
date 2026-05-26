#include "BytecodeText.h"

#include "InstructionPrint.h"

#include <sstream>
#include <variant>

namespace PickVM {
    namespace {
        std::string escapeQuoted(const std::string &input) {
            std::string out;
            out.reserve(input.size());
            for (const char c: input) {
                switch (c) {
                    case '\\': out += "\\\\"; break;
                    case '"': out += "\\\""; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default: out.push_back(c); break;
                }
            }
            return out;
        }
    } // namespace

    std::string serializeBytecodeText(const std::vector<Instruction> &program,
                                      const std::vector<int> &sourceLinePerInstr) {
        std::ostringstream out;
        if (!sourceLinePerInstr.empty()) {
            out << "#BASICLINES:";
            const std::size_t n = std::min(program.size(), sourceLinePerInstr.size());
            for (std::size_t i = 0; i < n; ++i) {
                if (i > 0) {
                    out << ',';
                }
                out << sourceLinePerInstr[i];
            }
            out << '\n';
        }

        for (const auto &instr: program) {
            out << opCodeName(instr.op);
            switch (instr.op) {
                case OpCode::PushInt:
                case OpCode::Jump:
                case OpCode::JumpIfZero:
                case OpCode::Call:
                    out << ' ' << std::get<int>(instr.operand);
                    break;
                case OpCode::PushFlt:
                    out << ' ' << std::get<double>(instr.operand);
                    break;
                case OpCode::PushStr:
                    out << " \"" << escapeQuoted(std::get<std::string>(instr.operand)) << '"';
                    break;
                case OpCode::LoadVar:
                case OpCode::StoreVar:
                case OpCode::ForSetup:
                case OpCode::ForNext:
                case OpCode::DimArray:
                case OpCode::LoadArr:
                case OpCode::StoreArr:
                case OpCode::MatInit:
                case OpCode::MatCopy:
                case OpCode::MatLoadFromRec:
                case OpCode::MatStoreToRec:
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
                case OpCode::ResolveDictAttr:
                case OpCode::CloseFile:
                    out << ' ' << std::get<std::string>(instr.operand);
                    break;
                case OpCode::InvokeBuiltin:
                    out << " \"" << escapeQuoted(std::get<std::string>(instr.operand)) << '"';
                    break;
                default:
                    break;
            }
            out << '\n';
        }
        return out.str();
    }
} // namespace PickVM
