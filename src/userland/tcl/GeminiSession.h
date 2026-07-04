//
// One running Gemini System session: owns VM runtime, session state, and Tcl shell front-end.
//

#ifndef PICK_SYSTEM_TCL_GEMINI_SESSION_H
#define PICK_SYSTEM_TCL_GEMINI_SESSION_H

#pragma once

#include <pickvm/core.hpp>

#include "FileSystem.h"
#include "Shell.h"
#include "TclEnvironment.h"
#include "UserSession.h"
#include "VocResolver.h"

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace PickCore::Locking {
    class LockTable;
} // namespace PickCore::Locking

namespace PickShell {
    class VmDebugService;

    class GeminiSession {
        friend class VmDebugService;
        friend class Shell;

    public:
        GeminiSession();

        [[nodiscard]] PickVM::Runtime &runtime() noexcept { return runtime_; }
        [[nodiscard]] const PickVM::Runtime &runtime() const noexcept { return runtime_; }

        [[nodiscard]] Shell &shell() noexcept { return shell_; }
        [[nodiscard]] const Shell &shell() const noexcept { return shell_; }

        [[nodiscard]] TclEnvironment &env() noexcept { return env_; }
        [[nodiscard]] const TclEnvironment &env() const noexcept { return env_; }

        [[nodiscard]] PickFS::FileSystem &fileSystem() noexcept { return fileSystem_; }
        [[nodiscard]] const PickFS::FileSystem &fileSystem() const noexcept { return fileSystem_; }

        [[nodiscard]] PickVoc::VocResolver &vocResolver() noexcept { return vocResolver_; }
        [[nodiscard]] const PickVoc::VocResolver &vocResolver() const noexcept { return vocResolver_; }

        /// nullptr selects process std::cin.
        void setInputStream(std::istream *in);
        /// nullptr selects process std::cout.
        void setOutputStream(std::ostream *out);
        /// nullptr selects process std::cerr.
        void setDiagnosticStream(std::ostream *err);

        [[nodiscard]] std::istream &input() const;
        [[nodiscard]] std::ostream &output() const;
        [[nodiscard]] std::ostream &diagnostic() const;

        [[nodiscard]] std::istream *inputStream() const noexcept { return inputStream_; }
        [[nodiscard]] std::ostream *outputStream() const noexcept { return outputStream_; }
        [[nodiscard]] std::ostream *diagnosticStream() const noexcept { return diagnosticStream_; }

        void setProgramsRoot(std::filesystem::path root);

        [[nodiscard]] const std::filesystem::path &programsRoot() const { return programsRoot_; }

        void setFileSystemRoot(std::filesystem::path root);

        [[nodiscard]] const std::filesystem::path &fileSystemRoot() const { return filesystemRoot_; }

        [[nodiscard]] const std::optional<std::string> &defaultDataFile() const { return defaultDataFile_; }

        [[nodiscard]] bool programImageLoaded() const;

        void pruneBreakpointsForProgram(std::size_t programSize, std::ostream &out);

        void executeVmLoop(std::ostream &out);

        void resetForQuit();

        void setGeminiCatalogRoot(std::optional<std::filesystem::path> root);

        [[nodiscard]] const std::optional<std::filesystem::path> &geminiCatalogRoot() const { return geminiCatalogRoot_; }

        /// Bind an authenticated account (replaces legacy applyUserSession).
        void attach(const PickCore::UserSession &session);

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

        [[nodiscard]] const std::optional<std::string> &activeListSourceFile() const { return activeListSourceFile_; }

        void setActiveList(std::vector<std::string> ids, std::string sourceFile);
        void clearActiveList();

        [[nodiscard]] int reportPageLength() const { return reportPageLength_; }
        void setReportPageLength(int n) { reportPageLength_ = n < 1 ? 1 : n; }

        [[nodiscard]] std::optional<std::string> resolveSystemVariable(std::string_view name) const;

        [[nodiscard]] static bool isSessionSystemVariableName(std::string_view name);

    private:
        void bindIoToShellAndRuntime();

        void reloadMdDefaultDataFile();
        void syncLockContextToFileSystem();
        void releaseSessionLocks();

        PickVM::Runtime runtime_;
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

        Shell shell_;

        std::istream *inputStream_{nullptr};
        std::ostream *outputStream_{nullptr};
        std::ostream *diagnosticStream_{nullptr};
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_GEMINI_SESSION_H
