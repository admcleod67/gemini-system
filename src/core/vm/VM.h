//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_VM_H
#define PICK_SYSTEM_VM_H

#include <string>
#include <variant>
#include <vector>
#include <unordered_map>

namespace PickVM {

    enum class OpCode {
        Halt,
        PushInt,
        PushStr,
        Add,
        Concat,
        PrintInt,
        PrintStr,
        Jump,
        JumpIfZero
    };

    using Value = std::variant<int, std::string>;

    struct Instruction {
        OpCode op;
        Value operand; // Only used for PUSH_INT, PUSH_STR, JUMP targets, etc.
    };

    class VM {
    public:
        VM();

        void loadProgram(const std::vector<Instruction> &program);

        void run();

        // Debug helper (optional)
        void dumpStack() const;

    private:
        std::vector<Instruction> program_;
        std::vector<Value> stack_;
        std::size_t ip_; // Instruction pointer

        // Helpers
        void push(const Value &v);

        Value pop();
    };

} // namespace PickVM

#endif //PICK_SYSTEM_VM_H
