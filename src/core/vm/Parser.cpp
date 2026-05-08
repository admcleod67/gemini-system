//
// Created by Allan McLeod on 18/04/2026.
//

#include "Parser.h"

#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace PickVM {
    namespace {
        std::string trimLocal(const std::string &s) {
            const size_t a = s.find_first_not_of(" \t\n\r");
            if (a == std::string::npos) {
                return "";
            }
            const size_t b = s.find_last_not_of(" \t\n\r");
            return s.substr(a, b - a + 1);
        }

        bool parseNonNegativeInt(const std::string &text, int &value) {
            try {
                std::size_t pos = 0;
                const long long v = std::stoll(text, &pos);
                if (pos != text.size() || v < 0 || v > std::numeric_limits<int>::max()) {
                    return false;
                }
                value = static_cast<int>(v);
                return true;
            } catch (const std::exception &) {
                return false;
            }
        }

        std::string parseQuotedString(const std::string &token, const int line) {
            if (token.size() < 2 || token.front() != '"' || token.back() != '"') {
                return token;
            }
            std::string out;
            out.reserve(token.size() - 2);
            for (std::size_t i = 1; i + 1 < token.size(); ++i) {
                const char c = token[i];
                if (c != '\\') {
                    out.push_back(c);
                    continue;
                }
                if (i + 1 >= token.size() - 1) {
                    throw std::runtime_error("Invalid string escape at line " + std::to_string(line));
                }
                const char e = token[++i];
                switch (e) {
                    case '\\': out.push_back('\\'); break;
                    case '"': out.push_back('"'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default:
                        throw std::runtime_error("Invalid string escape at line " + std::to_string(line));
                }
            }
            return out;
        }

        std::vector<int> parseBasicLinesMetadata(const std::string &line) {
            std::vector<int> out;
            std::string text = trimLocal(line);
            constexpr const char *prefix = "#BASICLINES:";
            if (text.rfind(prefix, 0) != 0) {
                return out;
            }
            const std::string csv = text.substr(std::char_traits<char>::length(prefix));
            if (csv.empty()) {
                return out;
            }
            std::istringstream iss(csv);
            std::string item;
            while (std::getline(iss, item, ',')) {
                item = trimLocal(item);
                if (item.empty()) {
                    throw std::runtime_error("Invalid #BASICLINES metadata");
                }
                int value = 0;
                if (!parseNonNegativeInt(item, value)) {
                    throw std::runtime_error("Invalid #BASICLINES metadata");
                }
                out.push_back(value);
            }
            return out;
        }

        void requireNoOperand(const ParsedLine &pl) {
            if (!pl.operand.empty()) {
                throw std::runtime_error(pl.opcode + " takes no operand at line " + std::to_string(pl.sourceLine));
            }
        }
    } // namespace

    std::string Parser::trim(const std::string &s) {
        size_t a = s.find_first_not_of(" \t\n\r");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\n\r");
        return s.substr(a, b - a + 1);
    }

    ParsedLine Parser::parseLine(const std::string &raw) {
        ParsedLine out;
        std::string line = trim(raw);

        if (line.empty() || line[0] == '#')
            return out;

        // Label?
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            out.label = trim(line.substr(0, colon));
            line = trim(line.substr(colon + 1));
            if (line.empty())
                return out;
        }

        std::istringstream iss(line);
        iss >> out.opcode;

        std::string rest;
        std::getline(iss, rest);
        out.operand = trim(rest);

        return out;
    }

    void Parser::validateJumpTargets(const std::vector<Instruction> &program) {
        for (std::size_t i = 0; i < program.size(); ++i) {
            const Instruction &inst = program[i];
            if (inst.op != OpCode::Jump && inst.op != OpCode::JumpIfZero && inst.op != OpCode::Call) {
                continue;
            }
            if (!std::holds_alternative<int>(inst.operand)) {
                throw std::runtime_error("Control-flow instruction at index " + std::to_string(i) + " has no int target");
            }
            const int t = std::get<int>(inst.operand);
            if (t < 0 || static_cast<std::size_t>(t) >= program.size()) {
                throw std::runtime_error("Control-flow target out of range at instruction " + std::to_string(i) + ": " + std::to_string(t));
            }
        }
    }

    LoadedBytecode Parser::parse(std::istream &in) {
        labelMap_.clear();
        intermediateLines_.clear();
        std::vector<int> metadataSourceLines;

        // Pass 1: read lines and collect labels
        {
            std::string line;
            int index = 0;
            int physLine = 0;
            while (std::getline(in, line)) {
                ++physLine;
                const std::string trimmed = trim(line);
                if (trimmed.rfind("#BASICLINES:", 0) == 0) {
                    metadataSourceLines = parseBasicLinesMetadata(trimmed);
                    continue;
                }
                ParsedLine pl = parseLine(line);
                if (!pl.label.empty()) {
                    labelMap_[pl.label] = index;
                }
                if (!pl.opcode.empty()) {
                    pl.sourceLine = physLine;
                    intermediateLines_.push_back(pl);
                    index++;
                }
            }
        }

        // Pass 2: convert to instructions
        LoadedBytecode loaded;
        loaded.labels = labelMap_;
        loaded.program.reserve(intermediateLines_.size());
        loaded.sourceLinePerInstr.reserve(intermediateLines_.size());

        for (const auto &pl: intermediateLines_) {
            Instruction inst;

            if (pl.opcode == "HALT") {
                inst.op = OpCode::Halt;
                requireNoOperand(pl);
            } else if (pl.opcode == "PUSH_INT") {
                inst.op = OpCode::PushInt;
                try {
                    inst.operand = std::stoi(pl.operand);
                } catch (const std::exception &) {
                    throw std::runtime_error(
                        "Invalid PUSH_INT operand at line " + std::to_string(pl.sourceLine) + ": '" + pl.operand + "'");
                }
            } else if (pl.opcode == "PUSH_FLT") {
                inst.op = OpCode::PushFlt;
                try {
                    std::size_t pos = 0;
                    const double v = std::stod(pl.operand, &pos);
                    if (pos != pl.operand.size()) {
                        throw std::runtime_error("partial");
                    }
                    inst.operand = v;
                } catch (const std::exception &) {
                    throw std::runtime_error(
                        "Invalid PUSH_FLT operand at line " + std::to_string(pl.sourceLine) + ": '" + pl.operand + "'");
                }
            } else if (pl.opcode == "PUSH_STR") {
                inst.op = OpCode::PushStr;
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "ADD") {
                inst.op = OpCode::Add;
                requireNoOperand(pl);
            } else if (pl.opcode == "SUB") {
                inst.op = OpCode::Sub;
                requireNoOperand(pl);
            } else if (pl.opcode == "MUL") {
                inst.op = OpCode::Mul;
                requireNoOperand(pl);
            } else if (pl.opcode == "DIV") {
                inst.op = OpCode::Div;
                requireNoOperand(pl);
            } else if (pl.opcode == "EQ") {
                inst.op = OpCode::Eq;
                requireNoOperand(pl);
            } else if (pl.opcode == "NE") {
                inst.op = OpCode::Ne;
                requireNoOperand(pl);
            } else if (pl.opcode == "LT") {
                inst.op = OpCode::Lt;
                requireNoOperand(pl);
            } else if (pl.opcode == "LE") {
                inst.op = OpCode::Le;
                requireNoOperand(pl);
            } else if (pl.opcode == "GT") {
                inst.op = OpCode::Gt;
                requireNoOperand(pl);
            } else if (pl.opcode == "GE") {
                inst.op = OpCode::Ge;
                requireNoOperand(pl);
            } else if (pl.opcode == "DUP") {
                inst.op = OpCode::Dup;
                requireNoOperand(pl);
            } else if (pl.opcode == "DROP") {
                inst.op = OpCode::Drop;
                requireNoOperand(pl);
            } else if (pl.opcode == "CONCAT") {
                inst.op = OpCode::Concat;
                requireNoOperand(pl);
            } else if (pl.opcode == "PRINT_INT") {
                inst.op = OpCode::PrintInt;
                requireNoOperand(pl);
            } else if (pl.opcode == "PRINT_STR") {
                inst.op = OpCode::PrintStr;
                requireNoOperand(pl);
            } else if (pl.opcode == "PRINT_VAL") {
                inst.op = OpCode::PrintVal;
                requireNoOperand(pl);
            } else if (pl.opcode == "PRINT_EOL") {
                inst.op = OpCode::PrintEol;
                requireNoOperand(pl);
            } else if (pl.opcode == "INPUT_INT") {
                inst.op = OpCode::InputInt;
                requireNoOperand(pl);
            } else if (pl.opcode == "INPUT_STR") {
                inst.op = OpCode::InputStr;
                requireNoOperand(pl);
            } else if (pl.opcode == "COERCE_INT") {
                inst.op = OpCode::CoerceInt;
                requireNoOperand(pl);
            } else if (pl.opcode == "JUMP") {
                inst.op = OpCode::Jump;
                int numericTarget = 0;
                if (parseNonNegativeInt(pl.operand, numericTarget)) {
                    inst.operand = numericTarget;
                } else {
                    try {
                        inst.operand = labelMap_.at(pl.operand);
                    } catch (const std::out_of_range &) {
                        throw std::runtime_error(
                            "Unknown label for JUMP at line " + std::to_string(pl.sourceLine) + ": " + pl.operand);
                    }
                }
            } else if (pl.opcode == "JZ") {
                inst.op = OpCode::JumpIfZero;
                int numericTarget = 0;
                if (parseNonNegativeInt(pl.operand, numericTarget)) {
                    inst.operand = numericTarget;
                } else {
                    try {
                        inst.operand = labelMap_.at(pl.operand);
                    } catch (const std::out_of_range &) {
                        throw std::runtime_error(
                            "Unknown label for JZ at line " + std::to_string(pl.sourceLine) + ": " + pl.operand);
                    }
                }
            } else if (pl.opcode == "CALL") {
                inst.op = OpCode::Call;
                int numericTarget = 0;
                if (parseNonNegativeInt(pl.operand, numericTarget)) {
                    inst.operand = numericTarget;
                } else {
                    try {
                        inst.operand = labelMap_.at(pl.operand);
                    } catch (const std::out_of_range &) {
                        throw std::runtime_error(
                            "Unknown label for CALL at line " + std::to_string(pl.sourceLine) + ": " + pl.operand);
                    }
                }
            } else if (pl.opcode == "RETURN") {
                inst.op = OpCode::Return;
                requireNoOperand(pl);
            } else if (pl.opcode == "FOR_SETUP") {
                inst.op = OpCode::ForSetup;
                if (pl.operand.empty()) {
                    throw std::runtime_error("FOR_SETUP requires a variable name at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "FOR_NEXT") {
                inst.op = OpCode::ForNext;
                if (pl.operand.empty()) {
                    throw std::runtime_error("FOR_NEXT requires a variable name at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "DIM_ARRAY") {
                inst.op = OpCode::DimArray;
                if (pl.operand.empty()) {
                    throw std::runtime_error("DIM_ARRAY requires a variable name at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "LOAD_ARR") {
                inst.op = OpCode::LoadArr;
                if (pl.operand.empty()) {
                    throw std::runtime_error("LOAD_ARR requires a variable name at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "STORE_ARR") {
                inst.op = OpCode::StoreArr;
                if (pl.operand.empty()) {
                    throw std::runtime_error("STORE_ARR requires a variable name at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "CLEAR_VARS") {
                inst.op = OpCode::ClearVars;
                requireNoOperand(pl);
            } else if (pl.opcode == "ABS_INT") {
                inst.op = OpCode::AbsInt;
                requireNoOperand(pl);
            } else if (pl.opcode == "SGN_INT") {
                inst.op = OpCode::SgnInt;
                requireNoOperand(pl);
            } else if (pl.opcode == "SEQ_STR") {
                inst.op = OpCode::SeqStr;
                requireNoOperand(pl);
            } else if (pl.opcode == "OPEN_FILE") {
                inst.op = OpCode::OpenFile;
                if (pl.operand.empty()) {
                    throw std::runtime_error("OPEN_FILE requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "OPEN_FILE_TRY") {
                inst.op = OpCode::OpenFileTry;
                if (pl.operand.empty()) {
                    throw std::runtime_error("OPEN_FILE_TRY requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "READ_REC") {
                inst.op = OpCode::ReadRec;
                if (pl.operand.empty()) {
                    throw std::runtime_error("READ_REC requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "READ_REC_TRY") {
                inst.op = OpCode::ReadRecTry;
                if (pl.operand.empty()) {
                    throw std::runtime_error("READ_REC_TRY requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "WRITE_REC") {
                inst.op = OpCode::WriteRec;
                if (pl.operand.empty()) {
                    throw std::runtime_error("WRITE_REC requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "WRITE_REC_TRY") {
                inst.op = OpCode::WriteRecTry;
                if (pl.operand.empty()) {
                    throw std::runtime_error("WRITE_REC_TRY requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "READ_NEXT") {
                inst.op = OpCode::ReadNext;
                if (pl.operand.empty()) {
                    throw std::runtime_error("READ_NEXT requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "READ_NEXT_TRY") {
                inst.op = OpCode::ReadNextTry;
                if (pl.operand.empty()) {
                    throw std::runtime_error("READ_NEXT_TRY requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "READ_V") {
                inst.op = OpCode::ReadV;
                if (pl.operand.empty()) {
                    throw std::runtime_error("READ_V requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "READ_V_TRY") {
                inst.op = OpCode::ReadVTry;
                if (pl.operand.empty()) {
                    throw std::runtime_error("READ_V_TRY requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "RESOLVE_DICT_ATTR") {
                inst.op = OpCode::ResolveDictAttr;
                if (pl.operand.empty()) {
                    throw std::runtime_error("RESOLVE_DICT_ATTR requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "WRITE_V") {
                inst.op = OpCode::WriteV;
                if (pl.operand.empty()) {
                    throw std::runtime_error("WRITE_V requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "WRITE_V_TRY") {
                inst.op = OpCode::WriteVTry;
                if (pl.operand.empty()) {
                    throw std::runtime_error("WRITE_V_TRY requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "EXTRACT_ATTR") {
                inst.op = OpCode::ExtractAttr;
                requireNoOperand(pl);
            } else if (pl.opcode == "CHAIN") {
                inst.op = OpCode::Chain;
                requireNoOperand(pl);
            } else if (pl.opcode == "CLOSE_FILE") {
                inst.op = OpCode::CloseFile;
                if (pl.operand.empty()) {
                    throw std::runtime_error("CLOSE_FILE requires a file variable at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "LOAD_VAR") {
                inst.op = OpCode::LoadVar;
                if (pl.operand.empty()) {
                    throw std::runtime_error(
                        "LOAD_VAR requires a variable name at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else if (pl.opcode == "STORE_VAR") {
                inst.op = OpCode::StoreVar;
                if (pl.operand.empty()) {
                    throw std::runtime_error(
                        "STORE_VAR requires a variable name at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = parseQuotedString(pl.operand, pl.sourceLine);
            } else {
                throw std::runtime_error(
                    "Unknown opcode at line " + std::to_string(pl.sourceLine) + ": " + pl.opcode);
            }

            loaded.program.push_back(inst);
        }

        if (!metadataSourceLines.empty()) {
            if (metadataSourceLines.size() != loaded.program.size()) {
                throw std::runtime_error("#BASICLINES metadata size mismatch");
            }
            loaded.sourceLinePerInstr = std::move(metadataSourceLines);
        } else {
            loaded.sourceLinePerInstr.reserve(intermediateLines_.size());
            for (const auto &pl: intermediateLines_) {
                loaded.sourceLinePerInstr.push_back(pl.sourceLine);
            }
        }

        validateJumpTargets(loaded.program);
        return loaded;
    }

    LoadedBytecode Parser::parseFile(const std::string &filename) {
        std::ifstream in(filename);
        if (!in)
            throw std::runtime_error("Cannot open bytecode file: " + filename);
        return parse(in);
    }
} // namespace PickVM
