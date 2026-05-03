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

        /// Directory containing ACCOUNTS.json and USERS.json (typically .../gemini).
        void setGeminiCatalogRoot(std::optional<std::filesystem::path> root);

        [[nodiscard]] const std::optional<std::filesystem::path> &geminiCatalogRoot() const { return geminiCatalogRoot_; }

        [[nodiscard]] bool loggedIn() const { return loggedIn_; }

        void setLoggedIn(bool value) { loggedIn_ = value; }

        void setSessionIdentity(int port, std::string username, std::string account);

        void clearLoginSession();

        [[nodiscard]] int whoPort() const { return whoPort_; }

        [[nodiscard]] const std::string &sessionUsername() const { return sessionUsername_; }

        [[nodiscard]] const std::string &sessionAccount() const { return sessionAccount_; }

        PickVM::Runtime &runtime_;
        TclEnvironment env_;
        std::filesystem::path programsRoot_{"programs"};
        std::filesystem::path filesystemRoot_{"filesystem"};
        PickFS::FileSystem fileSystem_;
        PickVoc::VocResolver vocResolver_;
        std::optional<std::filesystem::path> geminiCatalogRoot_;
        bool loggedIn_{false};
        int whoPort_{0};
        std::string sessionUsername_;
        std::string sessionAccount_;
        std::optional<PickVM::LoadedBytecode> lastLoaded_;
        bool trace_{false};
        std::unordered_set<std::size_t> breakpoints_;
        bool suspended_{false};
        std::optional<std::size_t> resumePastBreakpointIp_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_SHELLSESSION_H
