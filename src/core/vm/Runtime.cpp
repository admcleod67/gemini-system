//
// Created by Allan McLeod on 18/04/2026.
//

#include "Runtime.h"
#include "InstructionPrint.h"
#include "../filesystem/FileSystem.h"
#include "../filesystem/StructuredRecord.h"
#include "../filesystem/RecordAttribute.h"
#include "../english/DictionaryResolver.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <climits>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PickVM {
    namespace {
        enum class IoErrorKind {
            BackendUnavailable,
            MissingFile,
            FileVarNotOpen,
            MissingRecord,
            CursorExhausted,
            MissingAttribute,
            MissingSubvalue,
        };

        const char *ioErrorKindName(const IoErrorKind kind) {
            switch (kind) {
                case IoErrorKind::BackendUnavailable: return "BACKEND.UNAVAILABLE";
                case IoErrorKind::MissingFile: return "FILE.NOT.FOUND";
                case IoErrorKind::FileVarNotOpen: return "FILE.VAR.NOT.OPEN";
                case IoErrorKind::MissingRecord: return "RECORD.NOT.FOUND";
                case IoErrorKind::CursorExhausted: return "CURSOR.EXHAUSTED";
                case IoErrorKind::MissingAttribute: return "ATTRIBUTE.NOT.FOUND";
                case IoErrorKind::MissingSubvalue: return "SUBVALUE.NOT.FOUND";
            }
            return "IO.ERROR";
        }

        std::runtime_error makeIoError(const char *op, const IoErrorKind kind, const std::string &detail) {
            // Keep error messages short and stable; tests should prefer exception type/behavior.
            return std::runtime_error(std::string(op) + ": " + ioErrorKindName(kind) +
                                       (detail.empty() ? "" : " (" + detail + ")"));
        }

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

        // --- Value coercion contract (BASIC / InvokeBuiltin) ---
        // Numeric contexts (arithmetic, most builtin args, CoerceInt opcode): coerceToDouble / coerceToInt
        // use strtod-style prefix parsing; "" -> 0; "12ABC" parses as 12.0 (INT truncates toward zero).
        // Comparisons: compareValues — both int/double treated as numeric; both string lexicographic;
        // int vs string (etc.) -> type mismatch error.
        // Display and STR(): valueToString (float trimming rules; int decimal; strings as-is).
        // Implementations stay in this TU so builtins and opcodes stay consistent.

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

        // --- BASIC InvokeBuiltin registry (Milestone 7) ---
        // ICONV / OCONV: no conversion codes implemented yet; extend here when adding support.
        // Trigonometry: arguments are radians (std::sin / cos / tan).
        // DATE: internal day number (days since 1967-12-31, aligned so 1970-01-01 UTC = 732).
        // RND(): uniform double in [0, 1); RND(n) reseeds the generator with unsigned n then returns one sample.
        constexpr int kBuiltinSpaceMaxCount = 65536;
        constexpr int kPickInternalDateAtUnixEpoch = 732;

        using BuiltinStackFn = void (*)(std::vector<Value> &stack, Runtime *rt);

        struct BuiltinEntry {
            int arity; // >=0: exact pops; -1: RND accepts 0 or 1 value on stack before call
            BuiltinStackFn fn;
        };

        void builtinAbs(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            stack.push_back(std::abs(coerceToInt(v)));
        }

        void builtinSgn(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            const int x = coerceToInt(v);
            stack.push_back(x > 0 ? 1 : x < 0 ? -1 : 0);
        }

        void builtinSeq(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            const std::string s = valueToString(v);
            stack.push_back(s.empty() ? 0 : static_cast<int>(static_cast<unsigned char>(s[0])));
        }

        void builtinLen(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            stack.push_back(static_cast<int>(valueToString(v).size()));
        }

        void builtinTrim(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            const std::string s = valueToString(v);
            const std::size_t first = s.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                stack.push_back(std::string{});
            } else {
                const std::size_t last = s.find_last_not_of(" \t\r\n");
                stack.push_back(s.substr(first, last - first + 1));
            }
        }

        void builtinLcase(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            std::string s = valueToString(v);
            for (char &c : s) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            stack.push_back(std::move(s));
        }

        void builtinUcase(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            std::string s = valueToString(v);
            for (char &c : s) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            stack.push_back(std::move(s));
        }

        void builtinSpace(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            int n = coerceToInt(v);
            if (n < 0) {
                n = 0;
            }
            if (n > kBuiltinSpaceMaxCount) {
                throw std::runtime_error("BUILTIN: SPACE count exceeds limit");
            }
            stack.push_back(std::string(static_cast<std::size_t>(n), ' '));
        }

        void builtinInt(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            const double d = coerceToDouble(v);
            stack.push_back(static_cast<int>(d));
        }

        void builtinMod(std::vector<Value> &stack, Runtime *) {
            const Value bVal = stack.back();
            stack.pop_back();
            const Value aVal = stack.back();
            stack.pop_back();
            const double b = coerceToDouble(bVal);
            const double a = coerceToDouble(aVal);
            if (b == 0.0) {
                throw std::runtime_error("BUILTIN: MOD division by zero");
            }
            stack.push_back(std::fmod(a, b));
        }

        void builtinRnd(std::vector<Value> &stack, Runtime *) {
            thread_local std::mt19937 rng{std::random_device{}()};
            if (!stack.empty()) {
                rng.seed(static_cast<unsigned>(coerceToInt(stack.back())));
                stack.pop_back();
            }
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            stack.push_back(dist(rng));
        }

        void builtinSin(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            stack.push_back(std::sin(coerceToDouble(v)));
        }

        void builtinCos(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            stack.push_back(std::cos(coerceToDouble(v)));
        }

        void builtinTan(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            stack.push_back(std::tan(coerceToDouble(v)));
        }

        void builtinExp(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            stack.push_back(std::exp(coerceToDouble(v)));
        }

        void builtinLog(std::vector<Value> &stack, Runtime *) {
            Value v = stack.back();
            stack.pop_back();
            const double x = coerceToDouble(v);
            if (x <= 0.0) {
                throw std::runtime_error("BUILTIN: LOG domain");
            }
            stack.push_back(std::log(x));
        }

        void builtinDate(std::vector<Value> &stack, Runtime *) {
            const std::time_t now = std::time(nullptr);
            const auto dayUnix = static_cast<int>(now / 86400LL);
            stack.push_back(dayUnix + kPickInternalDateAtUnixEpoch);
        }

        void builtinTime(std::vector<Value> &stack, Runtime *) {
            const std::time_t now = std::time(nullptr);
            std::tm tmBuf{};
#if defined(_WIN32)
            gmtime_s(&tmBuf, &now);
#else
            gmtime_r(&now, &tmBuf);
#endif
            char buf[16]{};
            if (std::strftime(buf, sizeof buf, "%H:%M:%S", &tmBuf) == 0) {
                stack.push_back(std::string{"00:00:00"});
            } else {
                stack.push_back(std::string{buf});
            }
        }

        void builtinSystem(std::vector<Value> &stack, Runtime *rt) {
            Value v = stack.back();
            stack.pop_back();
            const int n = coerceToInt(v);
            stack.push_back(rt->evalBuiltinSystemCall(n));
        }

        // INDEX(S, substring [, occurrence]) — 1-based byte offset into UTF-8 bytes (same as LEN).
        // Omitted occurrence defaults to 1 at compile time (emitter pushes 1). Returns start
        // position of the nth match using overlapping search (next scan starts at found+1).
        // Returns 0 if fewer than n occurrences. Empty substring -> BUILTIN: INDEX empty substring;
        // occurrence < 1 -> BUILTIN: INDEX occurrence.
        void builtinIndex(std::vector<Value> &stack, Runtime *) {
            const Value occVal = stack.back();
            stack.pop_back();
            const Value needleVal = stack.back();
            stack.pop_back();
            const Value hayVal = stack.back();
            stack.pop_back();
            const std::string hay = valueToString(hayVal);
            const std::string needle = valueToString(needleVal);
            if (needle.empty()) {
                throw std::runtime_error("BUILTIN: INDEX empty substring");
            }
            const int occ = coerceToInt(occVal);
            if (occ < 1) {
                throw std::runtime_error("BUILTIN: INDEX occurrence");
            }
            std::size_t pos = 0;
            for (int i = 1; i <= occ; ++i) {
                const std::size_t found = hay.find(needle, pos);
                if (found == std::string::npos) {
                    stack.push_back(0);
                    return;
                }
                if (i == occ) {
                    stack.push_back(static_cast<int>(found + 1));
                    return;
                }
                pos = found + 1;
            }
            stack.push_back(0);
        }

        // FIELD(S, delimiter, n) — delimiter is a literal substring (not a regex). Fields are
        // split on each non-overlapping occurrence of delimiter; n is 1-based. n < 1 ->
        // BUILTIN: FIELD field index; empty delimiter -> BUILTIN: FIELD empty delimiter.
        // If n is past the last field, returns "".
        void builtinField(std::vector<Value> &stack, Runtime *) {
            const Value fieldIdxVal = stack.back();
            stack.pop_back();
            const Value delimVal = stack.back();
            stack.pop_back();
            const Value hayVal = stack.back();
            stack.pop_back();
            const std::string hay = valueToString(hayVal);
            const std::string delim = valueToString(delimVal);
            if (delim.empty()) {
                throw std::runtime_error("BUILTIN: FIELD empty delimiter");
            }
            const int fieldNum = coerceToInt(fieldIdxVal);
            if (fieldNum < 1) {
                throw std::runtime_error("BUILTIN: FIELD field index");
            }
            std::vector<std::string> fields;
            std::size_t start = 0;
            for (;;) {
                const std::size_t f = hay.find(delim, start);
                if (f == std::string::npos) {
                    fields.push_back(hay.substr(start));
                    break;
                }
                fields.push_back(hay.substr(start, f - start));
                start = f + delim.size();
            }
            if (static_cast<std::size_t>(fieldNum) > fields.size()) {
                stack.push_back(std::string{});
            } else {
                stack.push_back(fields[static_cast<std::size_t>(fieldNum - 1)]);
            }
        }

        // STR(x) — string form using the same rules as valueToString (int / double trimming / str).
        void builtinStr(std::vector<Value> &stack, Runtime *) {
            const Value v = stack.back();
            stack.pop_back();
            stack.push_back(valueToString(v));
        }

        const std::unordered_map<std::string, BuiltinEntry> &builtinRegistry() {
            static const std::unordered_map<std::string, BuiltinEntry> kRegistry{
                {"ABS", {1, builtinAbs}},
                {"SGN", {1, builtinSgn}},
                {"SEQ", {1, builtinSeq}},
                {"LEN", {1, builtinLen}},
                {"TRIM", {1, builtinTrim}},
                {"LCASE", {1, builtinLcase}},
                {"UCASE", {1, builtinUcase}},
                {"SPACE", {1, builtinSpace}},
                {"INT", {1, builtinInt}},
                {"MOD", {2, builtinMod}},
                {"RND", {-1, builtinRnd}},
                {"SIN", {1, builtinSin}},
                {"COS", {1, builtinCos}},
                {"TAN", {1, builtinTan}},
                {"EXP", {1, builtinExp}},
                {"LOG", {1, builtinLog}},
                {"DATE", {0, builtinDate}},
                {"TIME", {0, builtinTime}},
                {"SYSTEM", {1, builtinSystem}},
                {"INDEX", {3, builtinIndex}},
                {"FIELD", {3, builtinField}},
                {"STR", {1, builtinStr}},
            };
            return kRegistry;
        }

        void invokeBuiltinOnStack(const std::string &name, std::vector<Value> &stack, Runtime *rt) {
            const auto &reg = builtinRegistry();
            const auto it = reg.find(name);
            if (it == reg.end()) {
                throw std::runtime_error(std::string("BUILTIN: unknown ") + name);
            }
            const int arity = it->second.arity;
            if (arity >= 0) {
                if (static_cast<int>(stack.size()) < arity) {
                    throw std::runtime_error(std::string("BUILTIN: stack underflow for ") + name);
                }
            } else if (name == "RND") {
                if (static_cast<int>(stack.size()) > 1) {
                    throw std::runtime_error("BUILTIN: RND too many arguments");
                }
            }
            it->second.fn(stack, rt);
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

    void Runtime::setBuiltinSystemCallHandler(BuiltinSystemCallFn fn) {
        builtinSystemCallHandler_ = std::move(fn);
    }

    Value Runtime::evalBuiltinSystemCall(const int n) {
        if (!builtinSystemCallHandler_) {
            throw std::runtime_error("BUILTIN: SYSTEM not configured");
        }
        return builtinSystemCallHandler_(n);
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

            case OpCode::InvokeBuiltin: {
                const std::string name = stringOperandAtIp(instr, ip_);
                invokeBuiltinOnStack(name, stack_, this);
                break;
            }

            case OpCode::OpenFile: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const std::string fileName = valueToString(pop());
                if (!fileSystem_) {
                    throw makeIoError("OPEN", IoErrorKind::BackendUnavailable, "filesystem backend not configured");
                }
                try {
                    openFiles_[fileVar] = fileSystem_->openFile(fileName);
                    fileSystem_->resetReadNextCursor(openFiles_[fileVar]);
                } catch (const PickFS::FileSystemError &) {
                    throw makeIoError("OPEN", IoErrorKind::MissingFile, fileName);
                }
                break;
            }

            case OpCode::OpenFileTry: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const std::string fileName = valueToString(pop());
                if (!fileSystem_) {
                    push(0);
                    break;
                }
                try {
                    openFiles_[fileVar] = fileSystem_->openFile(fileName);
                    fileSystem_->resetReadNextCursor(openFiles_[fileVar]);
                } catch (const PickFS::FileSystemError &) {
                    push(0);
                    break;
                }
                push(1);
                break;
            }

            case OpCode::ReadRec: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const std::string id = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end()) {
                    throw makeIoError("READ", IoErrorKind::FileVarNotOpen, fileVar);
                }
                if (!fileSystem_) {
                    throw makeIoError("READ", IoErrorKind::BackendUnavailable, "filesystem backend not configured");
                }
                std::optional<std::string> value;
                const std::optional<PickFS::Record> rec = fileSystem_->read(fit->second.name, id);
                if (rec.has_value()) {
                    value = rec->value();
                }
                if (!value.has_value()) {
                    throw makeIoError("READ", IoErrorKind::MissingRecord, id);
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
                const std::optional<PickFS::Record> rec = fileSystem_->read(fit->second.name, id);
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
                    throw makeIoError("WRITE", IoErrorKind::FileVarNotOpen, fileVar);
                }
                if (!fileSystem_) {
                    throw makeIoError("WRITE", IoErrorKind::BackendUnavailable, "filesystem backend not configured");
                }
                fileSystem_->write(fit->second.name, PickFS::Record{id, value});
                fileSystem_->resetReadNextCursor(fit->second);
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
                    fileSystem_->write(fit->second.name, PickFS::Record{id, value});
                    fileSystem_->resetReadNextCursor(fit->second);
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
                    throw makeIoError("READNEXT", IoErrorKind::FileVarNotOpen, fileVar);
                }
                if (!fileSystem_) {
                    throw makeIoError("READNEXT", IoErrorKind::BackendUnavailable, "filesystem backend not configured");
                }
                try {
                    push(fileSystem_->readNextRecord(fit->second));
                } catch (const PickFS::FileSystemError &) {
                    throw makeIoError("READNEXT", IoErrorKind::CursorExhausted, "file=" + fit->second.name);
                }
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
                try {
                    push(fileSystem_->readNextRecord(fit->second));
                    push(1);
                } catch (const std::exception &) {
                    push(std::string{});
                    push(0);
                }
                break;
            }

            case OpCode::ReadV: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const int valueIndex = coerceToInt(pop());
                const int attrNo = coerceToInt(pop());
                const std::string id = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end()) {
                    throw makeIoError("READV", IoErrorKind::FileVarNotOpen, fileVar);
                }
                if (!fileSystem_) {
                    throw makeIoError("READV", IoErrorKind::BackendUnavailable, "filesystem backend not configured");
                }
                const std::optional<PickFS::Record> rec = fileSystem_->read(fit->second.name, id);
                if (!rec.has_value()) {
                    throw makeIoError("READV", IoErrorKind::MissingRecord, id);
                }
                const PickFS::StructuredRecord sr = PickFS::StructuredRecord::fromRaw(rec->value());
                if (!sr.hasAttribute(attrNo)) {
                    throw makeIoError("READV", IoErrorKind::MissingAttribute, "attr=" + std::to_string(attrNo));
                }
                const PickFS::RecordAttribute attr = sr.attribute(attrNo);
                if (valueIndex > 0) {
                    if (!attr.hasValueAt(valueIndex)) {
                        throw makeIoError("READV", IoErrorKind::MissingSubvalue,
                                           "attr=" + std::to_string(attrNo) + ",value=" + std::to_string(valueIndex));
                    }
                    push(attr.valueAt(valueIndex));
                } else {
                    // valueIndex == 0 → attribute-level read.
                    push(attr.raw());
                }
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
                const std::optional<PickFS::Record> rec = fileSystem_->read(fit->second.name, id);
                if (!rec.has_value()) {
                    push(std::string{});
                    push(0);
                    break;
                }
                const PickFS::StructuredRecord sr = PickFS::StructuredRecord::fromRaw(rec->value());
                if (!sr.hasAttribute(attrNo)) {
                    push(std::string{});
                    push(0);
                    break;
                }
                const PickFS::RecordAttribute attr = sr.attribute(attrNo);
                if (valueIndex > 0) {
                    if (!attr.hasValueAt(valueIndex)) {
                        push(std::string{});
                        push(0);
                        break;
                    }
                    push(attr.valueAt(valueIndex));
                    push(1);
                } else {
                    // valueIndex == 0 → attribute-level read.
                    push(attr.raw());
                    push(1);
                }
                break;
            }

            case OpCode::ExtractAttr: {
                const int valueIndex = coerceToInt(pop());
                const int attrNo = coerceToInt(pop());
                const std::string rawRecord = valueToString(pop());
                const PickFS::StructuredRecord sr = PickFS::StructuredRecord::fromRaw(rawRecord);
                const PickFS::RecordAttribute attr = sr.attribute(attrNo);
                push(valueIndex > 0 ? attr.valueAt(valueIndex) : attr.raw());
                break;
            }

            case OpCode::ResolveDictAttr: {
                // Pops a dictionary field-token string and resolves it into an attribute index
                // using DICT-<dataFile> precedence. Resolution failures push 0 (missing attribute)
                // instead of throwing so *TRY/ELSE stack contracts remain intact.
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const std::string token = valueToString(pop());
                int resolvedAttrNo = 0;
                if (fileSystem_) {
                    const auto fit = openFiles_.find(fileVar);
                    if (fit != openFiles_.end()) {
                        try {
                            PickCore::English::DictionaryResolver resolver;
                            const auto field = resolver.resolveField(*fileSystem_, fit->second.name, token);
                            if (field.attributeNo.has_value()) {
                                resolvedAttrNo = *field.attributeNo;
                            }
                        } catch (const std::exception &) {
                            resolvedAttrNo = 0;
                        }
                    }
                }
                push(resolvedAttrNo);
                break;
            }

            case OpCode::Chain: {
                const std::string programName = valueToString(pop());
                if (programName.empty()) {
                    throw std::runtime_error("CHAIN requires a program name");
                }
                throw ChainRequest{programName};
            }

            case OpCode::WriteV: {
                const std::string fileVar = canonicalVariableName(stringOperandAtIp(instr, ip_));
                const int valueIndex = coerceToInt(pop());
                const int attrNo = coerceToInt(pop());
                const std::string id = valueToString(pop());
                const std::string value = valueToString(pop());
                const auto fit = openFiles_.find(fileVar);
                if (fit == openFiles_.end()) {
                    throw makeIoError("WRITEV", IoErrorKind::FileVarNotOpen, fileVar);
                }
                if (!fileSystem_) {
                    throw makeIoError("WRITEV", IoErrorKind::BackendUnavailable, "filesystem backend not configured");
                }
                if (valueIndex > 0) {
                    fileSystem_->writeSubvalue(fit->second.name, id, attrNo, valueIndex, value);
                } else {
                    fileSystem_->writeAttributeValue(fit->second.name, id, attrNo, value);
                }
                fileSystem_->resetReadNextCursor(fit->second);
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
                    if (valueIndex > 0) {
                        fileSystem_->writeSubvalue(fit->second.name, id, attrNo, valueIndex, value);
                    } else {
                        fileSystem_->writeAttributeValue(fit->second.name, id, attrNo, value);
                    }
                    fileSystem_->resetReadNextCursor(fit->second);
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
