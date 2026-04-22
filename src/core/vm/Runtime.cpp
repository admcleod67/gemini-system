//
// Created by Allan McLeod on 18/04/2026.
//

#include "Runtime.h"
#include "InstructionPrint.h"

#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace PickVM {
    namespace {
        std::string canonicalVariableName(const std::string &raw) {
            std::string out(raw);
            for (char &c: out) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return out;
        }

        int intOperandAtIp(const Instruction &instr, std::size_t ip) {
            if (!std::holds_alternative<int>(instr.operand)) {
                std::ostringstream oss;
                oss << "Instruction " << ip << " (" << PickVM::opCodeName(instr.op) << "): expected int operand";
                throw std::runtime_error(oss.str());
            }
            return std::get<int>(instr.operand);
        }

        std::string stringOperandAtIp(const Instruction &instr, std::size_t ip) {
            if (!std::holds_alternative<std::string>(instr.operand)) {
                std::ostringstream oss;
                oss << "Instruction " << ip << " (" << PickVM::opCodeName(instr.op) << "): expected string operand";
                throw std::runtime_error(oss.str());
            }
            return std::get<std::string>(instr.operand);
        }

        int intFromStackValue(const Value &v, const char *ctx) {
            if (!std::holds_alternative<int>(v)) {
                throw std::runtime_error(std::string(ctx) + ": expected int on stack");
            }
            return std::get<int>(v);
        }

        std::string stringFromStackValue(const Value &v, const char *ctx) {
            if (!std::holds_alternative<std::string>(v)) {
                throw std::runtime_error(std::string(ctx) + ": expected string on stack");
            }
            return std::get<std::string>(v);
        }

        bool parseIntLine(const std::string &line, int &value) {
            std::size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return false;
            }
            std::size_t last = line.find_last_not_of(" \t\r\n");
            const std::string trimmed = line.substr(first, last - first + 1);
            std::size_t pos = 0;
            try {
                value = std::stoi(trimmed, &pos);
            } catch (const std::exception &) {
                return false;
            }
            return pos == trimmed.size();
        }
    } // namespace

    std::ostream &Runtime::out() const {
        return outStream_ ? *outStream_ : std::cout;
    }

    std::istream &Runtime::in() const {
        return inStream_ ? *inStream_ : std::cin;
    }

    void Runtime::setOutputStream(std::ostream *out) const {
        outStream_ = out;
    }

    void Runtime::setInputStream(std::istream *in) const {
        inStream_ = in;
    }

    Runtime::Runtime()
        : ip_(0) {
    }

    void Runtime::loadProgram(const std::vector<Instruction> &program) {
        program_ = program;
        ip_ = 0;
        stack_.clear();
        variables_.clear();
    }

    void Runtime::push(const Value &v) {
        stack_.push_back(v);
    }

    Value Runtime::pop() {
        if (stack_.empty()) {
            throw std::runtime_error("VM stack underflow");
        }
        Value v = stack_.back();
        stack_.pop_back();
        return v;
    }

    bool Runtime::step() {
        if (ip_ >= program_.size()) {
            return false;
        }

        const Instruction &instr = program_[ip_];

        switch (instr.op) {
            case OpCode::Halt:
                ip_ = program_.size();
                return false;

            case OpCode::PushInt:
                push(intOperandAtIp(instr, ip_));
                break;

            case OpCode::PushStr:
                push(stringOperandAtIp(instr, ip_));
                break;

            case OpCode::Add: {
                int b = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                int a = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                push(a + b);
                break;
            }

            case OpCode::Sub: {
                int b = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                int a = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                push(a - b);
                break;
            }

            case OpCode::Mul: {
                int b = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                int a = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                push(a * b);
                break;
            }

            case OpCode::Div: {
                int b = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                int a = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                if (b == 0) {
                    throw std::runtime_error("DIV: divide by zero");
                }
                push(a / b);
                break;
            }

            case OpCode::Eq: {
                int b = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                int a = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                push(a == b ? 1 : 0);
                break;
            }

            case OpCode::Ne: {
                int b = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                int a = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                push(a != b ? 1 : 0);
                break;
            }

            case OpCode::Lt: {
                int b = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                int a = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                push(a < b ? 1 : 0);
                break;
            }

            case OpCode::Le: {
                int b = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                int a = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                push(a <= b ? 1 : 0);
                break;
            }

            case OpCode::Gt: {
                int b = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                int a = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                push(a > b ? 1 : 0);
                break;
            }

            case OpCode::Ge: {
                int b = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                int a = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                push(a >= b ? 1 : 0);
                break;
            }

            case OpCode::Concat: {
                std::string b = stringFromStackValue(pop(), PickVM::opCodeName(instr.op));
                std::string a = stringFromStackValue(pop(), PickVM::opCodeName(instr.op));
                push(a + b);
                break;
            }

            case OpCode::Dup: {
                if (stack_.empty()) {
                    throw std::runtime_error("VM stack underflow");
                }
                push(stack_.back());
                break;
            }

            case OpCode::Drop: {
                pop();
                break;
            }

            case OpCode::PrintInt: {
                int v = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                out() << v;
                break;
            }

            case OpCode::PrintStr: {
                std::string v = stringFromStackValue(pop(), PickVM::opCodeName(instr.op));
                out() << v;
                break;
            }

            case OpCode::PrintEol: {
                out() << std::endl;
                break;
            }

            case OpCode::InputInt: {
                std::string line;
                if (!std::getline(in(), line)) {
                    throw std::runtime_error("INPUT_INT: end of input");
                }
                int value = 0;
                if (!parseIntLine(line, value)) {
                    throw std::runtime_error("INPUT_INT: invalid integer input");
                }
                push(value);
                break;
            }

            case OpCode::Jump: {
                int target = intOperandAtIp(instr, ip_);
                ip_ = static_cast<std::size_t>(target);
                return ip_ < program_.size();
            }

            case OpCode::JumpIfZero: {
                int target = intOperandAtIp(instr, ip_);
                int v = intFromStackValue(pop(), PickVM::opCodeName(instr.op));
                if (v == 0) {
                    ip_ = static_cast<std::size_t>(target);
                    return ip_ < program_.size();
                }
                break;
            }

            case OpCode::LoadVar: {
                const std::string name = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const auto it = variables_.find(name);
                if (it == variables_.end()) {
                    throw std::runtime_error("Undefined variable: " + name);
                }
                push(it->second);
                break;
            }

            case OpCode::StoreVar: {
                const std::string name = canonicalVariableName(stringOperandAtIp(instr, ip_));
                variables_[name] = pop();
                break;
            }

            default: {
                std::ostringstream oss;
                oss << "Unknown opcode at ip " << ip_;
                throw std::runtime_error(oss.str());
            }
        }

        ++ip_;
        return ip_ < program_.size();
    }

    void Runtime::run() {
        while (step()) {
        }
    }

    void Runtime::dumpStack() const {
        out() << "Stack: [ ";
        for (const auto &v: stack_) {
            if (std::holds_alternative<int>(v)) {
                out() << std::get<int>(v) << " ";
            } else {
                out() << "\"" << std::get<std::string>(v) << "\" ";
            }
        }
        out() << "]\n";
    }
} // namespace PickVM
