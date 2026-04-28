#ifndef PICK_SYSTEM_PROC_PROCINTERPRETER_H
#define PICK_SYSTEM_PROC_PROCINTERPRETER_H

#pragma once

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace PickShell {
    class ProcInterpreter {
    public:
        using TclBridgeFn = std::function<void(const std::string &, std::ostream &)>;

        bool runScript(const std::vector<std::string> &lines,
                       const std::vector<std::string> &params,
                       std::istream *inputStream,
                       std::ostream &out,
                       const TclBridgeFn &tclBridgeFn) const;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_PROC_PROCINTERPRETER_H
