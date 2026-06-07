#ifndef PICK_SYSTEM_PROC_PROCINTERPRETER_H
#define PICK_SYSTEM_PROC_PROCINTERPRETER_H

#pragma once

#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PickShell {
    enum class ProcLockOutcome {
        Success,
        Locked,
        MissingRecord,
        HardError,
    };

    class ProcInterpreter {
    public:
        using TclBridgeFn = std::function<void(const std::string &, std::ostream &)>;
        using SessionAtFn = std::function<std::optional<std::string>(std::string_view)>;
        using SelectFn = std::function<bool(const std::string &, std::string &)>;
        using ReadNextFn = std::function<std::optional<std::string>(std::string &)>;
        using ReadUFn = std::function<ProcLockOutcome(
            const std::string &file, const std::string &id, std::string &recordBody, std::string &hardError)>;
        using WriteUFn = std::function<ProcLockOutcome(
            const std::string &file, const std::string &id, const std::string &value, std::string &hardError)>;
        using ReleaseFn = std::function<bool(const std::string &file, const std::string &id, std::string &hardError)>;

        bool runScript(const std::vector<std::string> &lines,
                       const std::vector<std::string> &params,
                       std::istream *inputStream,
                       std::ostream &out,
                       const TclBridgeFn &tclBridgeFn,
                       const SessionAtFn &sessionAt = {},
                       const SelectFn &selectFn = {},
                       const ReadNextFn &readNextFn = {},
                       const ReadUFn &readUFn = {},
                       const WriteUFn &writeUFn = {},
                       const ReleaseFn &releaseFn = {}) const;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_PROC_PROCINTERPRETER_H
