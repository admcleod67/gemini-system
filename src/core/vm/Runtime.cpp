//
// Created by Allan McLeod on 18/04/2026.
//

#include "Runtime.h"
#include "InstructionPrint.h"
#include "../filesystem/FileSystem.h"
#include "../filesystem/StructuredRecord.h"
#include "../filesystem/RecordAttribute.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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

        bool isReadOnlySystemVariableName(const std::string &canonical) {
            return canonical == "@USERNO" || canonical == "@ACCOUNT" || canonical == "@LOGNAME" || canonical == "@DEFDATA";
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

        double floatOperandAtIp(const Instruction &instr, std::size_t ip) {
            if (!std::holds_alternative<double>(instr.operand)) {
                std::ostringstream oss;
                oss << "Instruction " << ip << " (" << PickVM::opCodeName(instr.op) << "): expected float operand";
                throw std::runtime_error(oss.str());
            }
            return std::get<double>(instr.operand);
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

        std::string valueToString(const Value &v) {
            if (std::holds_alternative<std::string>(v)) {
                return std::get<std::string>(v);
            }
            if (std::holds_alternative<double>(v)) {
                std::ostringstream oss;
                oss << std::setprecision(15) << std::get<double>(v);
                std::string out = oss.str();
                if (out.find('.') != std::string::npos) {
                    while (!out.empty() && out.back() == '0') {
                        out.pop_back();
                    }
                    if (!out.empty() && out.back() == '.') {
                        out.push_back('0');
                    }
                }
                return out;
            }
            return std::to_string(std::get<int>(v));
        }

        double coerceToDouble(const Value &v) {
            if (std::holds_alternative<int>(v)) {
                return static_cast<double>(std::get<int>(v));
            }
            if (std::holds_alternative<double>(v)) {
                return std::get<double>(v);
            }
            const std::string &s = std::get<std::string>(v);
            if (s.empty()) {
                return 0.0;
            }
            char *endp = nullptr;
            errno = 0;
            const double n = std::strtod(s.c_str(), &endp);
            if (endp == s.c_str() || errno != 0) {
                return 0.0;
            }
            return n;
        }

        // True when strtod consumed a prefix number but the string continues with non-whitespace
        // (e.g. "12ABC") so arithmetic must keep floating semantics for the result type.
        bool stringHasTrailingNonNumericJunk(const std::string &s) {
            if (s.empty()) {
                return false;
            }
            char *endp = nullptr;
            errno = 0;
            (void) std::strtod(s.c_str(), &endp);
            if (endp == s.c_str() || errno != 0) {
                return false;
            }
            while (*endp != '\0' && std::isspace(static_cast<unsigned char>(*endp))) {
                ++endp;
            }
            return *endp != '\0';
        }

        bool arithmeticOperandForcesDoubleResult(const Value &v) {
            if (std::holds_alternative<double>(v)) {
                return true;
            }
            if (std::holds_alternative<int>(v)) {
                return false;
            }
            return stringHasTrailingNonNumericJunk(std::get<std::string>(v));
        }

        Value arithmeticResultValue(const Value &a, const Value &b, double result) {
            const bool forceDouble =
                arithmeticOperandForcesDoubleResult(a) || arithmeticOperandForcesDoubleResult(b);
            if (!forceDouble) {
                if (result >= static_cast<double>(std::numeric_limits<int>::min()) &&
                    result <= static_cast<double>(std::numeric_limits<int>::max()) &&
                    result == std::floor(result)) {
                    return static_cast<int>(result);
                }
            }
            return result;
        }

        int coerceToInt(const Value &v) {
            const double n = coerceToDouble(v);
            if (n < static_cast<double>(std::numeric_limits<int>::min()) ||
                n > static_cast<double>(std::numeric_limits<int>::max())) {
                return 0;
            }
            return static_cast<int>(n);
        }

        bool isNumericValue(const Value &v) {
            return std::holds_alternative<int>(v) || std::holds_alternative<double>(v);
        }

        // Returns -1 / 0 / +1 for a < b / a == b / a > b, for matching types.
        int compareValues(const Value &a, const Value &b, const char *ctx) {
            if (isNumericValue(a) && isNumericValue(b)) {
                const double ia = coerceToDouble(a);
                const double ib = coerceToDouble(b);
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

        std::string joinValues(const std::vector<std::string> &values) {
            constexpr char kValueMark = static_cast<char>(0xFD);
            std::string out;
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i > 0) {
                    out.push_back(kValueMark);
                }
                out += values[i];
            }
            return out;
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
        loadProgram(program, {});
    }

    void Runtime::loadProgram(const std::vector<Instruction> &program, const std::vector<int> &sourceLinePerInstr) {
        program_ = program;
        sourceLinePerInstr_.assign(program_.size(), 0);
        if (!sourceLinePerInstr.empty()) {
            const std::size_t n = std::min(program_.size(), sourceLinePerInstr.size());
            for (std::size_t i = 0; i < n; ++i) {
                sourceLinePerInstr_[i] = sourceLinePerInstr[i];
            }
        }
        ip_ = 0;
        stack_.clear();
        callStack_.clear();
        forStack_.clear();
        openFiles_.clear();
        arrays_.clear();
        interrupted_.store(false, std::memory_order_relaxed);
        variables_.clear();
    }

    int Runtime::sourceLineAtInstruction(const std::size_t ip) const {
        if (ip >= sourceLinePerInstr_.size()) {
            return 0;
        }
        return sourceLinePerInstr_[ip];
    }

    int Runtime::currentSourceLine() const {
        return sourceLineAtInstruction(ip_);
    }

    void Runtime::setInstructionPointer(const std::size_t ip) {
        if (ip > program_.size()) {
            throw std::out_of_range("Instruction pointer out of range");
        }
        ip_ = ip;
    }

    void Runtime::setFileSystem(PickFS::FileSystem *fileSystem) {
        fileSystem_ = fileSystem;
    }

    void Runtime::setSystemVariableReader(SystemVarReaderFn fn) {
        systemVarReader_ = std::move(fn);
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

            case OpCode::PushFlt:
                push(floatOperandAtIp(instr, ip_));
                break;

            case OpCode::PushStr:
                push(stringOperandAtIp(instr, ip_));
                break;

            case OpCode::Add: {
                const Value b = pop();
                const Value a = pop();
                if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                    push(std::get<int>(a) + std::get<int>(b));
                } else {
                    push(arithmeticResultValue(
                        a, b, coerceToDouble(a) + coerceToDouble(b)));
                }
                break;
            }

            case OpCode::Sub: {
                const Value b = pop();
                const Value a = pop();
                if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                    push(std::get<int>(a) - std::get<int>(b));
                } else {
                    push(arithmeticResultValue(
                        a, b, coerceToDouble(a) - coerceToDouble(b)));
                }
                break;
            }

            case OpCode::Mul: {
                const Value b = pop();
                const Value a = pop();
                if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
                    push(std::get<int>(a) * std::get<int>(b));
                } else {
                    push(arithmeticResultValue(
                        a, b, coerceToDouble(a) * coerceToDouble(b)));
                }
                break;
            }

            case OpCode::Div: {
                const Value bVal = pop();
                const Value aVal = pop();
                const double b = coerceToDouble(bVal);
                const double a = coerceToDouble(aVal);
                if (b == 0.0) {
                    throw std::runtime_error("DIV: divide by zero");
                }
                if (std::holds_alternative<int>(aVal) && std::holds_alternative<int>(bVal)) {
                    push(static_cast<int>(a / b));
                } else {
                    push(a / b);
                }
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
                out() << valueToString(v);
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
                if (isReadOnlySystemVariableName(frame.varName)) {
                    throw std::runtime_error("Read-only system variable: " + frame.varName);
                }
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

            case OpCode::DimArray: {
                const std::string name = canonicalVariableName(stringOperandAtIp(instr, ip_));
                if (isReadOnlySystemVariableName(name)) {
                    throw std::runtime_error("Read-only system variable: " + name);
                }
                const int size = coerceToInt(pop());
                if (size < 1) {
                    throw std::runtime_error("DIM: size must be >= 1");
                }
                arrays_[name].assign(static_cast<std::size_t>(size), Value{std::string{""}});
                break;
            }

            case OpCode::LoadArr: {
                const std::string name = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const int idx = coerceToInt(pop());
                const auto it = arrays_.find(name);
                if (it == arrays_.end()) {
                    throw std::runtime_error("Array not dimensioned: " + name);
                }
                if (idx < 1 || static_cast<std::size_t>(idx) > it->second.size()) {
                    throw std::runtime_error("Array index out of bounds: " + name);
                }
                push(it->second[static_cast<std::size_t>(idx) - 1]);
                break;
            }

            case OpCode::StoreArr: {
                const std::string name = canonicalVariableName(stringOperandAtIp(instr, ip_));
                if (isReadOnlySystemVariableName(name)) {
                    throw std::runtime_error("Read-only system variable: " + name);
                }
                const int idx = coerceToInt(pop()); // index on top
                Value val = pop();                  // value underneath
                const auto it = arrays_.find(name);
                if (it == arrays_.end()) {
                    throw std::runtime_error("Array not dimensioned: " + name);
                }
                if (idx < 1 || static_cast<std::size_t>(idx) > it->second.size()) {
                    throw std::runtime_error("Array index out of bounds: " + name);
                }
                it->second[static_cast<std::size_t>(idx) - 1] = std::move(val);
                break;
            }

            case OpCode::ClearVars: {
                variables_.clear();
                arrays_.clear();
                break;
            }

            case OpCode::AbsInt: {
                push(std::abs(coerceToInt(pop())));
                break;
            }

            case OpCode::SgnInt: {
                const int v = coerceToInt(pop());
                push(v > 0 ? 1 : v < 0 ? -1 : 0);
                break;
            }

            case OpCode::SeqStr: {
                const Value v = pop();
                const std::string s = valueToString(v);
                push(s.empty() ? 0 : static_cast<int>(static_cast<unsigned char>(s[0])));
                break;
            }

            case OpCode::OpenFile: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const std::string fileName = valueToString(pop());
                if (!fileSystem_) {
                    throw std::runtime_error("OPEN failed: filesystem backend not configured");
                }
                bool exists = false;
                try {
                    (void) fileSystem_->openFile(fileName);
                    exists = true;
                } catch (const PickFS::FileSystemError &) {
                    exists = false;
                }
                if (!exists) {
                    throw std::runtime_error("OPEN failed: file not found: " + fileName);
                }
                openFiles_[fileVar] = OpenFileState{fileName, {}, 0, false};
                break;
            }

            case OpCode::OpenFileTry: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const std::string fileName = valueToString(pop());
                if (!fileSystem_) {
                    push(0);
                    break;
                }
                bool exists = false;
                try {
                    (void) fileSystem_->openFile(fileName);
                    exists = true;
                } catch (const PickFS::FileSystemError &) {
                    exists = false;
                }
                if (!exists) {
                    push(0);
                    break;
                }
                openFiles_[fileVar] = OpenFileState{fileName, {}, 0, false};
                push(1);
                break;
            }

            case OpCode::ReadRec: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const std::string id = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end()) {
                    throw std::runtime_error("READ failed: file variable not open: " + fileVar);
                }
                if (!fileSystem_) {
                    throw std::runtime_error("READ failed: filesystem backend not configured");
                }
                std::optional<std::string> value;
                const std::optional<PickFS::Record> rec = fileSystem_->read(fit->second.fileName, id);
                if (rec.has_value()) {
                    value = rec->value();
                }
                if (!value.has_value()) {
                    throw std::runtime_error("READ failed: record not found: " + id);
                }
                push(*value);
                break;
            }

            case OpCode::ReadRecTry: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const std::string id = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end()) {
                    push(std::string{});
                    push(0);
                    break;
                }
                if (!fileSystem_) {
                    push(std::string{});
                    push(0);
                    break;
                }
                std::optional<std::string> value;
                const std::optional<PickFS::Record> rec = fileSystem_->read(fit->second.fileName, id);
                if (rec.has_value()) {
                    value = rec->value();
                }
                if (!value.has_value()) {
                    push(std::string{});
                    push(0);
                    break;
                }
                push(*value);
                push(1);
                break;
            }

            case OpCode::WriteRec: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const std::string id = valueToString(pop());
                const std::string value = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end()) {
                    throw std::runtime_error("WRITE failed: file variable not open: " + fileVar);
                }
                if (!fileSystem_) {
                    throw std::runtime_error("WRITE failed: filesystem backend not configured");
                }
                fileSystem_->write(fit->second.fileName, PickFS::Record{id, value});
                fit->second.cursorPrimed = false;
                fit->second.cursorIndex = 0;
                break;
            }

            case OpCode::WriteRecTry: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const std::string id = valueToString(pop());
                const std::string value = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end()) {
                    push(0);
                    break;
                }
                if (!fileSystem_) {
                    push(0);
                    break;
                }
                try {
                    fileSystem_->write(fit->second.fileName, PickFS::Record{id, value});
                    fit->second.cursorPrimed = false;
                    fit->second.cursorIndex = 0;
                    push(1);
                } catch (const std::exception &) {
                    push(0);
                }
                break;
            }

            case OpCode::ReadNext: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end()) {
                    throw std::runtime_error("READNEXT failed: file variable not open: " + fileVar);
                }
                if (!fileSystem_) {
                    throw std::runtime_error("READNEXT requires filesystem backend");
                }
                auto &state = fit->second;
                if (!state.cursorPrimed) {
                    state.recordIds = fileSystem_->listRecordNames(state.fileName);
                    state.cursorIndex = 0;
                    state.cursorPrimed = true;
                }
                if (state.cursorIndex >= state.recordIds.size()) {
                    throw std::runtime_error("READNEXT failed: cursor exhausted");
                }
                push(state.recordIds[state.cursorIndex++]);
                break;
            }

            case OpCode::ReadNextTry: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end() || !fileSystem_) {
                    push(std::string{});
                    push(0);
                    break;
                }
                auto &state = fit->second;
                if (!state.cursorPrimed) {
                    state.recordIds = fileSystem_->listRecordNames(state.fileName);
                    state.cursorIndex = 0;
                    state.cursorPrimed = true;
                }
                if (state.cursorIndex >= state.recordIds.size()) {
                    push(std::string{});
                    push(0);
                    break;
                }
                push(state.recordIds[state.cursorIndex++]);
                push(1);
                break;
            }

            case OpCode::ReadV: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const int valueIndex = coerceToInt(pop());
                const int attrNo = coerceToInt(pop());
                const std::string id = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end()) {
                    throw std::runtime_error("READV failed: file variable not open: " + fileVar);
                }
                if (!fileSystem_) {
                    throw std::runtime_error("READV requires filesystem backend");
                }
                const std::optional<PickFS::Record> rec = fileSystem_->read(fit->second.fileName, id);
                if (!rec.has_value()) {
                    throw std::runtime_error("READV failed: record not found: " + id);
                }
                const PickFS::StructuredRecord sr = PickFS::StructuredRecord::fromRaw(rec->value());
                const PickFS::RecordAttribute attr = sr.attribute(attrNo);
                push(valueIndex > 0 ? attr.valueAt(valueIndex) : attr.raw());
                break;
            }

            case OpCode::ReadVTry: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const int valueIndex = coerceToInt(pop());
                const int attrNo = coerceToInt(pop());
                const std::string id = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end() || !fileSystem_) {
                    push(std::string{});
                    push(0);
                    break;
                }
                const std::optional<PickFS::Record> rec = fileSystem_->read(fit->second.fileName, id);
                if (!rec.has_value()) {
                    push(std::string{});
                    push(0);
                    break;
                }
                const PickFS::StructuredRecord sr = PickFS::StructuredRecord::fromRaw(rec->value());
                const PickFS::RecordAttribute attr = sr.attribute(attrNo);
                push(valueIndex > 0 ? attr.valueAt(valueIndex) : attr.raw());
                push(1);
                break;
            }

            case OpCode::WriteV: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const int valueIndex = coerceToInt(pop());
                const int attrNo = coerceToInt(pop());
                const std::string id = valueToString(pop());
                const std::string value = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end()) {
                    throw std::runtime_error("WRITEV failed: file variable not open: " + fileVar);
                }
                if (!fileSystem_) {
                    throw std::runtime_error("WRITEV requires filesystem backend");
                }
                PickFS::StructuredRecord sr{};
                if (const std::optional<PickFS::Record> rec = fileSystem_->read(fit->second.fileName, id); rec.has_value()) {
                    sr = PickFS::StructuredRecord::fromRaw(rec->value());
                }
                if (valueIndex > 0) {
                    std::vector<std::string> values = sr.attribute(attrNo).splitValues();
                    if (values.empty()) {
                        values.push_back("");
                    }
                    if (static_cast<std::size_t>(valueIndex) > values.size()) {
                        values.resize(static_cast<std::size_t>(valueIndex), "");
                    }
                    values[static_cast<std::size_t>(valueIndex - 1)] = value;
                    sr.setAttribute(attrNo, PickFS::RecordAttribute{joinValues(values)});
                } else {
                    sr.setAttribute(attrNo, PickFS::RecordAttribute{value});
                }
                fileSystem_->write(fit->second.fileName, PickFS::Record{id, sr.toRaw()});
                auto &state = fit->second;
                state.cursorPrimed = false;
                state.cursorIndex = 0;
                break;
            }

            case OpCode::WriteVTry: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const int valueIndex = coerceToInt(pop());
                const int attrNo = coerceToInt(pop());
                const std::string id = valueToString(pop());
                const std::string value = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end() || !fileSystem_) {
                    push(0);
                    break;
                }
                try {
                    PickFS::StructuredRecord sr{};
                    if (const std::optional<PickFS::Record> rec = fileSystem_->read(fit->second.fileName, id); rec.has_value()) {
                        sr = PickFS::StructuredRecord::fromRaw(rec->value());
                    }
                    if (valueIndex > 0) {
                        std::vector<std::string> values = sr.attribute(attrNo).splitValues();
                        if (values.empty()) {
                            values.push_back("");
                        }
                        if (static_cast<std::size_t>(valueIndex) > values.size()) {
                            values.resize(static_cast<std::size_t>(valueIndex), "");
                        }
                        values[static_cast<std::size_t>(valueIndex - 1)] = value;
                        sr.setAttribute(attrNo, PickFS::RecordAttribute{joinValues(values)});
                    } else {
                        sr.setAttribute(attrNo, PickFS::RecordAttribute{value});
                    }
                    fileSystem_->write(fit->second.fileName, PickFS::Record{id, sr.toRaw()});
                    auto &state = fit->second;
                    state.cursorPrimed = false;
                    state.cursorIndex = 0;
                    push(1);
                } catch (const std::exception &) {
                    push(0);
                }
                break;
            }

            case OpCode::CloseFile: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                openFiles_.erase(fileVar); // no-op if not open
                break;
            }

            case OpCode::LoadVar: {
                const std::string name = canonicalVariableName(stringOperandAtIp(instr, ip_));
                if (systemVarReader_ && isReadOnlySystemVariableName(name)) {
                    if (const std::optional<Value> sv = systemVarReader_(std::string_view(name))) {
                        push(*sv);
                        break;
                    }
                }
                const auto it = variables_.find(name);
                if (it == variables_.end()) {
                    throw std::runtime_error("Undefined variable: " + name);
                }
                push(it->second);
                break;
            }

            case OpCode::StoreVar: {
                const std::string name = canonicalVariableName(stringOperandAtIp(instr, ip_));
                if (isReadOnlySystemVariableName(name)) {
                    throw std::runtime_error("Read-only system variable: " + name);
                }
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
            if (std::holds_alternative<std::string>(v)) {
                out() << "\"" << std::get<std::string>(v) << "\" ";
            } else {
                out() << valueToString(v) << " ";
            }
        }
        out() << "]\n";
    }
} // namespace PickVM
