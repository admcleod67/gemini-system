//
// Created by Allan McLeod on 18/04/2026.
//

#include "VM.h"

#include <iostream>
#include <stdexcept>

namespace PickVM {

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
                    push(std::get<int>(instr.operand));
                    break;

                case OpCode::PushStr:
                    push(std::get<std::string>(instr.operand));
                    break;

                case OpCode::Add: {
                    int b = std::get<int>(pop());
                    int a = std::get<int>(pop());
                    push(a + b);
                    break;
                }

                case OpCode::Concat: {
                    std::string b = std::get<std::string>(pop());
                    std::string a = std::get<std::string>(pop());
                    push(a + b);
                    break;
                }

                case OpCode::PrintInt: {
                    int v = std::get<int>(pop());
                    std::cout << v << std::endl;
                    break;
                }

                case OpCode::PrintStr: {
                    std::string v = std::get<std::string>(pop());
                    std::cout << v << std::endl;
                    break;
                }

                case OpCode::Jump: {
                    int target = std::get<int>(instr.operand);
                    ip_ = static_cast<std::size_t>(target);
                    continue;
                }

                case OpCode::JumpIfZero: {
                    int target = std::get<int>(instr.operand);
                    int v = std::get<int>(pop());
                    if (v == 0) {
                        ip_ = static_cast<std::size_t>(target);
                        continue;
                    }
                    break;
                }

                default:
                    throw std::runtime_error("Unknown opcode");
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
