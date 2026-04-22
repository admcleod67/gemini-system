//
// Created by Allan McLeod on 18/04/2026.
//

#include "Parser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace PickVM {
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
            if (inst.op != OpCode::Jump && inst.op != OpCode::JumpIfZero) {
                continue;
            }
            if (!std::holds_alternative<int>(inst.operand)) {
                throw std::runtime_error(
                    "Jump instruction at index " + std::to_string(i) + " has no int target");
            }
            const int t = std::get<int>(inst.operand);
            if (t < 0 || static_cast<std::size_t>(t) >= program.size()) {
                throw std::runtime_error(
                    "Jump target out of range at instruction " + std::to_string(i) + ": " + std::to_string(t));
            }
        }
    }

    LoadedBytecode Parser::parse(std::istream &in) {
        labelMap_.clear();
        intermediateLines_.clear();

        // Pass 1: read lines and collect labels
        {
            std::string line;
            int index = 0;
            int physLine = 0;
            while (std::getline(in, line)) {
                ++physLine;
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
            } else if (pl.opcode == "PUSH_INT") {
                inst.op = OpCode::PushInt;
                try {
                    inst.operand = std::stoi(pl.operand);
                } catch (const std::exception &) {
                    throw std::runtime_error(
                        "Invalid PUSH_INT operand at line " + std::to_string(pl.sourceLine) + ": '" + pl.operand + "'");
                }
            } else if (pl.opcode == "PUSH_STR") {
                inst.op = OpCode::PushStr;
                std::string s = pl.operand;
                if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                    s = s.substr(1, s.size() - 2);
                inst.operand = s;
            } else if (pl.opcode == "ADD") {
                inst.op = OpCode::Add;
            } else if (pl.opcode == "SUB") {
                inst.op = OpCode::Sub;
            } else if (pl.opcode == "MUL") {
                inst.op = OpCode::Mul;
            } else if (pl.opcode == "DIV") {
                inst.op = OpCode::Div;
            } else if (pl.opcode == "EQ") {
                inst.op = OpCode::Eq;
            } else if (pl.opcode == "NE") {
                inst.op = OpCode::Ne;
            } else if (pl.opcode == "LT") {
                inst.op = OpCode::Lt;
            } else if (pl.opcode == "LE") {
                inst.op = OpCode::Le;
            } else if (pl.opcode == "GT") {
                inst.op = OpCode::Gt;
            } else if (pl.opcode == "GE") {
                inst.op = OpCode::Ge;
            } else if (pl.opcode == "DUP") {
                inst.op = OpCode::Dup;
            } else if (pl.opcode == "DROP") {
                inst.op = OpCode::Drop;
            } else if (pl.opcode == "CONCAT") {
                inst.op = OpCode::Concat;
            } else if (pl.opcode == "PRINT_INT") {
                inst.op = OpCode::PrintInt;
            } else if (pl.opcode == "PRINT_STR") {
                inst.op = OpCode::PrintStr;
            } else if (pl.opcode == "INPUT_INT") {
                inst.op = OpCode::InputInt;
                if (!pl.operand.empty()) {
                    throw std::runtime_error(
                        "INPUT_INT takes no operand at line " + std::to_string(pl.sourceLine));
                }
            } else if (pl.opcode == "JUMP") {
                inst.op = OpCode::Jump;
                try {
                    inst.operand = labelMap_.at(pl.operand);
                } catch (const std::out_of_range &) {
                    throw std::runtime_error(
                        "Unknown label for JUMP at line " + std::to_string(pl.sourceLine) + ": " + pl.operand);
                }
            } else if (pl.opcode == "JZ") {
                inst.op = OpCode::JumpIfZero;
                try {
                    inst.operand = labelMap_.at(pl.operand);
                } catch (const std::out_of_range &) {
                    throw std::runtime_error(
                        "Unknown label for JZ at line " + std::to_string(pl.sourceLine) + ": " + pl.operand);
                }
            } else if (pl.opcode == "LOAD_VAR") {
                inst.op = OpCode::LoadVar;
                if (pl.operand.empty()) {
                    throw std::runtime_error(
                        "LOAD_VAR requires a variable name at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = pl.operand;
            } else if (pl.opcode == "STORE_VAR") {
                inst.op = OpCode::StoreVar;
                if (pl.operand.empty()) {
                    throw std::runtime_error(
                        "STORE_VAR requires a variable name at line " + std::to_string(pl.sourceLine));
                }
                inst.operand = pl.operand;
            } else {
                throw std::runtime_error(
                    "Unknown opcode at line " + std::to_string(pl.sourceLine) + ": " + pl.opcode);
            }

            loaded.sourceLinePerInstr.push_back(pl.sourceLine);
            loaded.program.push_back(inst);
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
