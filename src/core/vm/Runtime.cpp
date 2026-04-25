//
// Created by Allan McLeod on 18/04/2026.
//

#include "Runtime.h"
#include "InstructionPrint.h"

#include <cctype>
#include <climits>
#include <iostream>
#include <limits>
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

        // Coerce a Value to int following Pick semantics: ints pass through,
        // strings are parsed as decimal integers; non-numeric strings yield 0.
        int coerceToInt(const Value &v) {
            if (std::holds_alternative<int>(v)) {
                return std::get<int>(v);
            }
            const std::string &s = std::get<std::string>(v);
            if (s.empty()) {
                return 0;
            }
            char *endp = nullptr;
            errno = 0;
            const long n = std::strtol(s.c_str(), &endp, 10);
            // skip trailing whitespace
            while (*endp == ' ' || *endp == '\t') {
                ++endp;
            }
            if (*endp != '\0' || errno != 0) {
                return 0;
            }
            if (n < static_cast<long>(std::numeric_limits<int>::min()) ||
                n > static_cast<long>(std::numeric_limits<int>::max())) {
                return 0;
            }
            return static_cast<int>(n);
        }

        // Returns -1 / 0 / +1 for a < b / a == b / a > b, for matching types.
        int compareValues(const Value &a, const Value &b, const char *ctx) {
            if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                const int ia = std::get<int>(a), ib = std::get<int>(b);
                if (ia < ib) return -1;
                if (ia > ib) return 1;
                return 0;
            }
            if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
                const auto &sa = std::get<std::string>(a), &sb = std::get<std::string>(b);
                if (sa < sb) return -1;
                if (sa > sb) return 1;
                return 0;
            }
            throw std::runtime_error(std::string(ctx) + ": type mismatch");
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
        callStack_.clear();
        forStack_.clear();
        interrupted_.store(false, std::memory_order_relaxed);
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
                int b = coerceToInt(pop());
                int a = coerceToInt(pop());
                push(a + b);
                break;
            }

            case OpCode::Sub: {
                int b = coerceToInt(pop());
                int a = coerceToInt(pop());
                push(a - b);
                break;
            }

            case OpCode::Mul: {
                int b = coerceToInt(pop());
                int a = coerceToInt(pop());
                push(a * b);
                break;
            }

            case OpCode::Div: {
                int b = coerceToInt(pop());
                int a = coerceToInt(pop());
                if (b == 0) {
                    throw std::runtime_error("DIV: divide by zero");
                }
                push(a / b);
                break;
            }

            case OpCode::Eq: {
                Value b = pop();
                Value a = pop();
                push(compareValues(a, b, "EQ") == 0 ? 1 : 0);
                break;
            }

            case OpCode::Ne: {
                Value b = pop();
                Value a = pop();
                push(compareValues(a, b, "NE") != 0 ? 1 : 0);
                break;
            }

            case OpCode::Lt: {
                Value b = pop();
                Value a = pop();
                push(compareValues(a, b, "LT") < 0 ? 1 : 0);
                break;
            }

            case OpCode::Le: {
                Value b = pop();
                Value a = pop();
                push(compareValues(a, b, "LE") <= 0 ? 1 : 0);
                break;
            }

            case OpCode::Gt: {
                Value b = pop();
                Value a = pop();
                push(compareValues(a, b, "GT") > 0 ? 1 : 0);
                break;
            }

            case OpCode::Ge: {
                Value b = pop();
                Value a = pop();
                push(compareValues(a, b, "GE") >= 0 ? 1 : 0);
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

            case OpCode::PrintVal: {
                const Value v = pop();
                if (std::holds_alternative<int>(v)) {
                    out() << std::get<int>(v);
                } else {
                    out() << std::get<std::string>(v);
                }
                break;
            }

            case OpCode::CoerceInt: {
                push(coerceToInt(pop()));
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

            case OpCode::InputStr: {
                std::string line;
                if (!std::getline(in(), line)) {
                    throw std::runtime_error("INPUT_STR: end of input");
                }
                const std::size_t first = line.find_first_not_of(" \t\r\n");
                if (first == std::string::npos) {
                    push(std::string{});
                } else {
                    const std::size_t last = line.find_last_not_of(" \t\r\n");
                    push(line.substr(first, last - first + 1));
                }
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

            case OpCode::Call: {
                int target = intOperandAtIp(instr, ip_);
                callStack_.push_back(ip_ + 1);
                ip_ = static_cast<std::size_t>(target);
                return ip_ < program_.size();
            }

            case OpCode::Return: {
                if (callStack_.empty()) {
                    throw std::runtime_error("RETURN without GOSUB");
                }
                ip_ = callStack_.back();
                callStack_.pop_back();
                return ip_ < program_.size();
            }

            case OpCode::ForSetup: {
                const std::string varName = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const int step  = coerceToInt(pop());
                const int limit = coerceToInt(pop());
                if (step == 0) {
                    throw std::runtime_error("FOR: STEP cannot be zero");
                }
                forStack_.push_back({varName, limit, step, ip_ + 1});
                break; // falls through to ++ip_ → body
            }

            case OpCode::ForNext: {
                const std::string varName = canonicalVariableName(stringOperandAtIp(instr, ip_));
                if (forStack_.empty()) {
                    throw std::runtime_error("NEXT without FOR");
                }
                ForFrame &frame = forStack_.back();
                // Only validate the name when one was provided; bare NEXT matches the innermost frame.
                if (!varName.empty() && frame.varName != varName) {
                    throw std::runtime_error("FOR/NEXT variable mismatch");
                }
                const auto it = variables_.find(frame.varName);
                const int cur = (it != variables_.end()) ? coerceToInt(it->second) : 0;
                const int newVal = cur + frame.step;
                variables_[frame.varName] = newVal;
                const bool done = (frame.step > 0) ? (newVal > frame.limit)
                                                   : (newVal < frame.limit);
                if (done) {
                    forStack_.pop_back();
                    break; // falls through to ++ip_ → code after loop
                }
                ip_ = frame.bodyIP;
                return ip_ < program_.size();
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
            if (interrupted_.load(std::memory_order_relaxed)) {
                throw UserInterrupt{};
            }
        }
    }

    void Runtime::interrupt() noexcept {
        interrupted_.store(true, std::memory_order_relaxed);
    }

    void Runtime::clearInterrupt() noexcept {
        interrupted_.store(false, std::memory_order_relaxed);
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
