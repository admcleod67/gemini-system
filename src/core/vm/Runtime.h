//
// Created by Allan McLeod on 18/04/2026.
//

#ifndef PICK_SYSTEM_VM_RUNTIME_H
#define PICK_SYSTEM_VM_RUNTIME_H

#include <atomic>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace PickFS {
    class FileSystem;
}

// Required because Runtime stores PickFS::FileSystem::FileHandle in an owning container.
#include "../filesystem/FileSystem.h"

namespace PickVM {
    enum class OpCode {
        Halt,
        PushInt,
        PushFlt,
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
        ForSetup,
        ForNext,
        DimArray,
        LoadArr,
        StoreArr,
        MatInit,
        MatCopy,
        MatLoadFromRec,
        MatStoreToRec,
        ClearVars,
        AbsInt,
        SgnInt,
        SeqStr,
        InvokeBuiltin,
        OpenFile,
        OpenFileTry,
        ReadRec,
        ReadRecTry,
        ReadRecU,
        ReadRecUTry,
        WriteRec,
        WriteRecTry,
        WriteRecU,
        WriteRecUTry,
        ReleaseRec,
        ReadNext,
        ReadNextTry,
        ReadV,
        ReadVTry,
        WriteV,
        WriteVTry,
        ExtractAttr,
        ResolveDictAttr,
        Chain,
        CloseFile,
        SetOnErrorHandler,
        LoadVar,
        StoreVar
    };

    /// BASIC STATUS() value when a record lock conflict is handled or raised (Milestone 10).
    constexpr int kStatusRecordLocked = 1;

    using Value = std::variant<int, double, std::string>;

    // Thrown by Runtime::run() when the interrupt flag is set (e.g. via Ctrl-C).
    struct UserInterrupt {};

    // Thrown when a BASIC CHAIN requests transfer to another program.
    struct ChainRequest {
        std::string programName;
    };

    struct ForFrame {
        std::string varName;
        int limit{0};
        int step{0};
        std::size_t bodyIP{0};
    };

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
        void loadProgram(const std::vector<Instruction> &program, const std::vector<int> &sourceLinePerInstr);

        // Execute until halt or instruction pointer past end (same observable behavior as before).
        // Throws UserInterrupt if interrupt() has been called.
        void run();

        // Single instruction. Returns false when execution stops (HALT or no more instructions).
        bool step();

        std::size_t instructionPointer() const { return ip_; }
        void setInstructionPointer(std::size_t ip);
        int currentSourceLine() const;
        int sourceLineAtInstruction(std::size_t ip) const;

        bool isLoaded() const { return !program_.empty(); }

        // Signal the running program to stop; safe to call from a signal handler.
        void interrupt() noexcept;
        void clearInterrupt() noexcept;

        void setFileSystem(PickFS::FileSystem *fileSystem);

        /// Optional read-only `@USERNO` / `@ACCOUNT` / `@LOGNAME` / `@DEFDATA` (canonical names). Cleared with empty function.
        using SystemVarReaderFn = std::function<std::optional<Value>(std::string_view canonicalName)>;
        void setSystemVariableReader(SystemVarReaderFn fn);

        /// Optional handler for BASIC `SYSTEM(n)` via `InvokeBuiltin` (Milestone 7 Stage 3). Cleared with empty function.
        using BuiltinSystemCallFn = std::function<Value(int n)>;
        void setBuiltinSystemCallHandler(BuiltinSystemCallFn fn);

        /// Invoked from builtin dispatch for `SYSTEM`; throws if no handler was set.
        [[nodiscard]] Value evalBuiltinSystemCall(int n);

        // Debug helper (optional)
        void dumpStack() const;

        // Read-only stack view for tests and diagnostics
        const std::vector<Value> &stack() const { return stack_; }

        [[nodiscard]] int statusCode() const { return statusCode_; }

    private:
        std::vector<Instruction> program_;
        std::vector<int> sourceLinePerInstr_; // parallel to program_; 0 means "no source line"
        std::vector<Value> stack_;
        std::vector<std::size_t> callStack_; // Return-address stack for GOSUB/RETURN
        std::vector<ForFrame> forStack_;     // Loop-frame stack for FOR/NEXT
        std::unordered_map<std::string, Value> variables_;
        std::unordered_map<std::string, std::vector<Value>> arrays_; // DIM arrays
        // file var -> opened filesystem handle (cursor state lives in PickFS::FileHandle)
        std::unordered_map<std::string, PickFS::FileSystem::FileHandle> openFiles_;
        std::size_t ip_; // Instruction pointer
        std::atomic<bool> interrupted_{false};
        mutable std::ostream *outStream_{nullptr};
        mutable std::istream *inStream_{nullptr};
        PickFS::FileSystem *fileSystem_{nullptr};
        SystemVarReaderFn systemVarReader_;
        BuiltinSystemCallFn builtinSystemCallHandler_;
        int statusCode_{0};
        std::optional<std::size_t> onErrorIp_;

        std::ostream &out() const;
        std::istream &in() const;

        [[nodiscard]] bool handleRecordLockConflict(const char *op);

        // Helpers
        void push(const Value &v);

        Value pop();
    };
} // namespace PickVM

#endif // PICK_SYSTEM_VM_RUNTIME_H
