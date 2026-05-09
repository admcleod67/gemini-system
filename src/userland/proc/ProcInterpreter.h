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
    class ProcInterpreter {
    public:
        using TclBridgeFn = std::function<void(const std::string &, std::ostream &)>;
        using SessionAtFn = std::function<std::optional<std::string>(std::string_view)>;
        using SelectFn = std::function<bool(const std::string &, std::string &)>;
        using ReadNextFn = std::function<std::optional<std::string>(std::string &)>;

        bool runScript(const std::vector<std::string> &lines,
                       const std::vector<std::string> &params,
                       std::istream *inputStream,
                       std::ostream &out,
                       const TclBridgeFn &tclBridgeFn,
                       const SessionAtFn &sessionAt = {},
                       const SelectFn &selectFn = {},
                       const ReadNextFn &readNextFn = {}) const;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_PROC_PROCINTERPRETER_H
