//
// Created by Allan McLeod on 18/04/2026.
//

#include "VM.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace PickVM {

namespace {

const char* opCodeName(OpCode op) {
    switch (op) {
        case OpCode::Halt: return "HALT";
        case OpCode::PushInt: return "PUSH_INT";
        case OpCode::PushStr: return "PUSH_STR";
        case OpCode::Add: return "ADD";
        case OpCode::Concat: return "CONCAT";
        case OpCode::PrintInt: return "PRINT_INT";
        case OpCode::PrintStr: return "PRINT_STR";
        case OpCode::Jump: return "JUMP";
        case OpCode::JumpIfZero: return "JZ";
    }
    return "UNKNOWN";
}

int intOperandAtIp(const Instruction& instr, std::size_t ip) {
    if (!std::holds_alternative<int>(instr.operand)) {
        std::ostringstream oss;
        oss << "Instruction " << ip << " (" << opCodeName(instr.op) << "): expected int operand";
        throw std::runtime_error(oss.str());
    }
    return std::get<int>(instr.operand);
}

std::string stringOperandAtIp(const Instruction& instr, std::size_t ip) {
    if (!std::holds_alternative<std::string>(instr.operand)) {
        std::ostringstream oss;
        oss << "Instruction " << ip << " (" << opCodeName(instr.op) << "): expected string operand";
        throw std::runtime_error(oss.str());
    }
    return std::get<std::string>(instr.operand);
}

int intFromStackValue(const Value& v, const char* ctx) {
    if (!std::holds_alternative<int>(v)) {
        throw std::runtime_error(std::string(ctx) + ": expected int on stack");
    }
    return std::get<int>(v);
}

std::string stringFromStackValue(const Value& v, const char* ctx) {
    if (!std::holds_alternative<std::string>(v)) {
        throw std::runtime_error(std::string(ctx) + ": expected string on stack");
    }
    return std::get<std::string>(v);
}

} // namespace

    VM::VM()
        : ip_(0) {
    }

    void VM::loadProgram(const std::vector<Instruction> &program) {
        program_ = program;
        ip_ = 0;
        stack_.clear();
    }

    void VM::push(const Value &v) {
        stack_.push_back(v);
    }

    Value VM::pop() {
        if (stack_.empty()) {
            throw std::runtime_error("VM stack underflow");
        }
        Value v = stack_.back();
        stack_.pop_back();
        return v;
    }

    void VM::run() {
        while (ip_ < program_.size()) {
            const Instruction &instr = program_[ip_];

            switch (instr.op) {
                case OpCode::Halt:
                    return;

                case OpCode::PushInt:
                    push(intOperandAtIp(instr, ip_));
                    break;

                case OpCode::PushStr:
                    push(stringOperandAtIp(instr, ip_));
                    break;

                case OpCode::Add: {
                    int b = intFromStackValue(pop(), "ADD");
                    int a = intFromStackValue(pop(), "ADD");
                    push(a + b);
                    break;
                }

                case OpCode::Concat: {
                    std::string b = stringFromStackValue(pop(), "CONCAT");
                    std::string a = stringFromStackValue(pop(), "CONCAT");
                    push(a + b);
                    break;
                }

                case OpCode::PrintInt: {
                    int v = intFromStackValue(pop(), "PRINT_INT");
                    std::cout << v << std::endl;
                    break;
                }

                case OpCode::PrintStr: {
                    std::string v = stringFromStackValue(pop(), "PRINT_STR");
                    std::cout << v << std::endl;
                    break;
                }

                case OpCode::Jump: {
                    int target = intOperandAtIp(instr, ip_);
                    ip_ = static_cast<std::size_t>(target);
                    continue;
                }

                case OpCode::JumpIfZero: {
                    int target = intOperandAtIp(instr, ip_);
                    int v = intFromStackValue(pop(), "JZ");
                    if (v == 0) {
                        ip_ = static_cast<std::size_t>(target);
                        continue;
                    }
                    break;
                }

                default: {
                    std::ostringstream oss;
                    oss << "Unknown opcode at ip " << ip_;
                    throw std::runtime_error(oss.str());
                }
            }

            ip_++;
        }
    }

    void VM::dumpStack() const {
        std::cout << "Stack: [ ";
        for (const auto &v: stack_) {
            if (std::holds_alternative<int>(v)) {
                std::cout << std::get<int>(v) << " ";
            } else {
                std::cout << "\"" << std::get<std::string>(v) << "\" ";
            }
        }
        std::cout << "]\n";
    }

} // namespace PickVM
