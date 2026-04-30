//
// Shared mutable shell session state.
//

#ifndef PICK_SYSTEM_TCL_SHELLSESSION_H
#define PICK_SYSTEM_TCL_SHELLSESSION_H

#pragma once

#include <pickvm/core.hpp>

#include "FileSystem.h"
#include "TclEnvironment.h"
#include "VocResolver.h"

#include <filesystem>
#include <optional>
#include <ostream>
#include <unordered_set>
#include <vector>

namespace PickShell {
    class ShellSession {
    public:
        explicit ShellSession(PickVM::Runtime &runtime);

        void setProgramsRoot(std::filesystem::path root);

        [[nodiscard]] const std::filesystem::path &programsRoot() const { return programsRoot_; }

        void setFileSystemRoot(std::filesystem::path root);

        [[nodiscard]] const std::filesystem::path &fileSystemRoot() const { return filesystemRoot_; }

        [[nodiscard]] bool programImageLoaded() const;

        void pruneBreakpointsForProgram(std::size_t programSize, std::ostream &out);

        void executeVmLoop(std::ostream &out);

        void resetForQuit();

        PickVM::Runtime &runtime_;
        TclEnvironment env_;
        std::filesystem::path programsRoot_{"programs"};
        std::filesystem::path filesystemRoot_{"filesystem"};
        PickFS::FileSystem fileSystem_;
        VocResolver vocResolver_;
        std::optional<PickVM::LoadedBytecode> lastLoaded_;
        bool trace_{false};
        std::unordered_set<std::size_t> breakpoints_;
        bool suspended_{false};
        std::optional<std::size_t> resumePastBreakpointIp_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_SHELLSESSION_H
