//
// Shared mutable shell session state.
//

#ifndef PICK_SYSTEM_TCL_SHELLSESSION_H
#define PICK_SYSTEM_TCL_SHELLSESSION_H

#pragma once

#include <pickvm/core.hpp>

#include "UserSession.h"

#include "FileSystem.h"
#include "TclEnvironment.h"
#include "VocResolver.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace PickCore::Locking {
    class LockTable;
} // namespace PickCore::Locking

namespace PickShell {
    class ShellSession {
    public:
        explicit ShellSession(PickVM::Runtime &runtime);

        void setProgramsRoot(std::filesystem::path root);

        [[nodiscard]] const std::filesystem::path &programsRoot() const { return programsRoot_; }

        void setFileSystemRoot(std::filesystem::path root);

        [[nodiscard]] const std::filesystem::path &fileSystemRoot() const { return filesystemRoot_; }

        /// Logical Pick file from `MD,DEFDATA` when set (after each `setFileSystemRoot`; cleared on `clearLoginSession`).
        [[nodiscard]] const std::optional<std::string> &defaultDataFile() const { return defaultDataFile_; }

        [[nodiscard]] bool programImageLoaded() const;

        void pruneBreakpointsForProgram(std::size_t programSize, std::ostream &out);

        void executeVmLoop(std::ostream &out);

        void resetForQuit();

        /// Directory containing ACCOUNTS.json and USERS.json (typically .../gemini).
        void setGeminiCatalogRoot(std::optional<std::filesystem::path> root);

        [[nodiscard]] const std::optional<std::filesystem::path> &geminiCatalogRoot() const { return geminiCatalogRoot_; }

        /// Apply catalogue login result from core (`PickCore::LoginService`).
        void applyUserSession(const PickCore::UserSession &session);

        [[nodiscard]] bool loggedIn() const { return loggedIn_; }

        void setLoggedIn(bool value) { loggedIn_ = value; }

        void setSessionIdentity(int port, std::string username, std::string account);

        void clearLoginSession();

        void setSharedLockTable(std::shared_ptr<PickCore::Locking::LockTable> table);

        [[nodiscard]] static std::string makeSessionLockId(int port,
                                                           const std::string &account,
                                                           const std::string &username);

        void bindLockSession(const std::string &sessionLockId);

        [[nodiscard]] const std::string &sessionLockId() const { return sessionLockId_; }

        [[nodiscard]] int whoPort() const { return whoPort_; }

        [[nodiscard]] const std::string &sessionUsername() const { return sessionUsername_; }

        [[nodiscard]] const std::string &sessionAccount() const { return sessionAccount_; }

        [[nodiscard]] const std::vector<std::string> &activeList() const { return activeList_; }

        /// File that `SELECT` / active-list ids refer to (logical Pick file name).
        [[nodiscard]] const std::optional<std::string> &activeListSourceFile() const { return activeListSourceFile_; }

        void setActiveList(std::vector<std::string> ids, std::string sourceFile);
        void clearActiveList();

        /// Page length used by the ENGLISH formatter for `HEADING` reports.
        /// Configured via the Tcl `SET PAGE-LENGTH n` command (M8 Stage 2).
        /// Default = 24 (Pick terminal-class); reset on `clearLoginSession`.
        [[nodiscard]] int reportPageLength() const { return reportPageLength_; }
        void setReportPageLength(int n) { reportPageLength_ = n < 1 ? 1 : n; }

        /// `@USERNO`, `@ACCOUNT`, `@LOGNAME`, `@DEFDATA` — read from session frame (not `TclEnvironment`).
        [[nodiscard]] std::optional<std::string> resolveSystemVariable(std::string_view name) const;

        [[nodiscard]] static bool isSessionSystemVariableName(std::string_view name);

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
        std::string userNo_{"0"};
        std::optional<std::string> defaultDataFile_;
        std::vector<std::string> activeList_;
        std::optional<std::string> activeListSourceFile_;
        std::optional<PickVM::LoadedBytecode> lastLoaded_;
        int reportPageLength_{24};
        bool trace_{false};
        std::unordered_set<std::size_t> breakpoints_;
        bool suspended_{false};
        std::optional<std::size_t> resumePastBreakpointIp_;
        std::shared_ptr<PickCore::Locking::LockTable> lockTable_;
        std::string sessionLockId_;

        void reloadMdDefaultDataFile();
        void syncLockContextToFileSystem();
        void releaseSessionLocks();
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_SHELLSESSION_H
