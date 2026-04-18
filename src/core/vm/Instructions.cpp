//
// Created by Allan McLeod on 18/04/2026.
//

#include "Instructions.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace PickVM {

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\n\r");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\n\r");
    return s.substr(a, b - a + 1);
}

ParsedLine parseLine(const std::string& raw) {
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

std::vector<Instruction> loadTextBytecode(const std::string& filename) {
    std::ifstream in(filename);
    if (!in)
        throw std::runtime_error("Cannot open bytecode file: " + filename);

    std::vector<ParsedLine> lines;
    std::unordered_map<std::string, int> labelMap;

    // Pass 1: read lines and collect labels
    {
        std::string line;
        int index = 0;
        while (std::getline(in, line)) {
            ParsedLine pl = parseLine(line);
            if (!pl.label.empty()) {
                labelMap[pl.label] = index;
            }
            if (!pl.opcode.empty()) {
                lines.push_back(pl);
                index++;
            }
        }
    }

    // Pass 2: convert to instructions
    std::vector<Instruction> program;
    program.reserve(lines.size());

    for (const auto& pl : lines) {
        Instruction inst;

        if (pl.opcode == "HALT") {
            inst.op = OpCode::Halt;
        }
        else if (pl.opcode == "PUSH_INT") {
            inst.op = OpCode::PushInt;
            inst.operand = std::stoi(pl.operand);
        }
        else if (pl.opcode == "PUSH_STR") {
            inst.op = OpCode::PushStr;
            std::string s = pl.operand;
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                s = s.substr(1, s.size() - 2);
            inst.operand = s;
        }
        else if (pl.opcode == "ADD") {
            inst.op = OpCode::Add;
        }
        else if (pl.opcode == "CONCAT") {
            inst.op = OpCode::Concat;
        }
        else if (pl.opcode == "PRINT_INT") {
            inst.op = OpCode::PrintInt;
        }
        else if (pl.opcode == "PRINT_STR") {
            inst.op = OpCode::PrintStr;
        }
        else if (pl.opcode == "JUMP") {
            inst.op = OpCode::Jump;
            inst.operand = labelMap.at(pl.operand);
        }
        else if (pl.opcode == "JZ") {
            inst.op = OpCode::JumpIfZero;
            inst.operand = labelMap.at(pl.operand);
        }
        else {
            throw std::runtime_error("Unknown opcode: " + pl.opcode);
        }

        program.push_back(inst);
    }

    return program;
}

} // namespace PickVM
