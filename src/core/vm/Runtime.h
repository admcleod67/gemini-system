//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_VM_RUNTIME_H
#define PICK_SYSTEM_VM_RUNTIME_H

#include <iosfwd>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace PickVM {
    enum class OpCode {
        Halt,
        PushInt,
        PushStr,
        Add,
        Sub,
        Mul,
        Div,
        Eq,
        Ne,
        Lt,
        Le,
        Gt,
        Ge,
        Concat,
        Dup,
        Drop,
        PrintInt,
        PrintStr,
        PrintVal,
        PrintEol,
        InputInt,
        InputStr,
        CoerceInt,
        Jump,
        JumpIfZero,
        Call,
        Return,
        LoadVar,
        StoreVar
    };

    using Value = std::variant<int, std::string>;

    struct Instruction {
        OpCode op;
        Value operand; // Only used for PUSH_INT, PUSH_STR, JUMP targets, etc.
    };

    class Runtime {
    public:
        Runtime();

        // nullptr selects std::cout (default). Used by tests to capture PRINT / dumpStack.
        void setOutputStream(std::ostream *out) const;

        // nullptr selects std::cin (default). Used by tests to inject INPUT.
        void setInputStream(std::istream *in) const;

        void loadProgram(const std::vector<Instruction> &program);

        // Execute until halt or instruction pointer past end (same observable behavior as before).
        void run();

        // Single instruction. Returns false when execution stops (HALT or no more instructions).
        bool step();

        std::size_t instructionPointer() const { return ip_; }

        bool isLoaded() const { return !program_.empty(); }

        // Debug helper (optional)
        void dumpStack() const;

        // Read-only stack view for tests and diagnostics
        const std::vector<Value> &stack() const { return stack_; }

    private:
        std::vector<Instruction> program_;
        std::vector<Value> stack_;
        std::vector<std::size_t> callStack_; // Return-address stack for GOSUB/RETURN
        std::unordered_map<std::string, Value> variables_;
        std::size_t ip_; // Instruction pointer
        mutable std::ostream *outStream_{nullptr};
        mutable std::istream *inStream_{nullptr};

        std::ostream &out() const;
        std::istream &in() const;

        // Helpers
        void push(const Value &v);

        Value pop();
    };
} // namespace PickVM

#endif // PICK_SYSTEM_VM_RUNTIME_H
