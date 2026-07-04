#include "GeminiSession.h"

#include "LockTable.h"
#include "MdDefaultDataFile.h"
#include "TclEnvironment.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <vector>

namespace PickShell {
    GeminiSession::GeminiSession()
        : fileSystem_(filesystemRoot_),
          vocResolver_(fileSystem_),
          shell_(*this) {
        bindIoToShellAndRuntime();
    }

    void GeminiSession::setInputStream(std::istream *const in) {
        inputStream_ = in;
        bindIoToShellAndRuntime();
    }

    void GeminiSession::setOutputStream(std::ostream *const out) {
        outputStream_ = out;
        bindIoToShellAndRuntime();
    }

    void GeminiSession::setDiagnosticStream(std::ostream *const err) {
        diagnosticStream_ = err;
    }

    std::istream &GeminiSession::input() const {
        return inputStream_ != nullptr ? *inputStream_ : std::cin;
    }

    std::ostream &GeminiSession::output() const {
        return outputStream_ != nullptr ? *outputStream_ : std::cout;
    }

    std::ostream &GeminiSession::diagnostic() const {
        return diagnosticStream_ != nullptr ? *diagnosticStream_ : std::cerr;
    }

    void GeminiSession::bindIoToShellAndRuntime() {
        shell_.setInputStream(inputStream_);
        shell_.setOutputStream(outputStream_);
        runtime_.setInputStream(inputStream_);
        runtime_.setOutputStream(outputStream_);
    }

    void GeminiSession::setProgramsRoot(std::filesystem::path root) {
        programsRoot_ = std::move(root);
    }

    void GeminiSession::setFileSystemRoot(std::filesystem::path root) {
        filesystemRoot_ = std::move(root);
        fileSystem_.setRoot(filesystemRoot_);
        vocResolver_.invalidate();
        reloadMdDefaultDataFile();
        clearActiveList();
        syncLockContextToFileSystem();
    }

    bool GeminiSession::programImageLoaded() const {
        return lastLoaded_.has_value() && runtime_.isLoaded();
    }

    void GeminiSession::pruneBreakpointsForProgram(const std::size_t programSize, std::ostream &out) {
        std::vector<std::size_t> removed;
        for (auto it = breakpoints_.begin(); it != breakpoints_.end();) {
            if (*it >= programSize) {
                removed.push_back(*it);
                it = breakpoints_.erase(it);
            } else {
                ++it;
            }
        }
        if (!removed.empty()) {
            std::sort(removed.begin(), removed.end());
            out << "Removed invalid breakpoint(s):";
            for (const std::size_t r: removed) {
                out << ' ' << r;
            }
            out << '\n';
        }
    }

    void GeminiSession::executeVmLoop(std::ostream &out) {
        if (!lastLoaded_) {
            return;
        }
        const auto &prog = lastLoaded_->program;
        while (runtime_.instructionPointer() < prog.size()) {
            const std::size_t ip = runtime_.instructionPointer();
            const bool skipBpOnce = resumePastBreakpointIp_ && *resumePastBreakpointIp_ == ip;
            if (skipBpOnce) {
                resumePastBreakpointIp_.reset();
            }

            if (!skipBpOnce && breakpoints_.count(ip) != 0U) {
                out << "Breakpoint hit at " << ip << '\n';
                suspended_ = true;
                break;
            }

            if (trace_) {
                const PickVM::Instruction &instr = prog[ip];
                out << PickVM::formatInstructionLine(ip, instr, &*lastLoaded_) << '\n';
            }

            if (!runtime_.step()) {
                suspended_ = false;
                break;
            }
        }
    }

    void GeminiSession::reset() {
        releaseSessionLocks();
        lastLoaded_.reset();
        suspended_ = false;
        resumePastBreakpointIp_.reset();
        breakpoints_.clear();
        trace_ = false;
        env_.clear();
        clearActiveList();
        runtime_.loadProgram({});
    }

    void GeminiSession::detach() {
        releaseSessionLocks();
        bindLockSession("");
        loggedIn_ = false;
        if (!daemonPortAssigned_) {
            whoPort_ = 0;
        }
        sessionUsername_.clear();
        sessionAccount_.clear();
        userNo_ = "0";
        defaultDataFile_.reset();
        reportPageLength_ = 24;
        clearActiveList();
    }

    void GeminiSession::destroy() {
        if (loggedIn_) {
            detach();
        }
        reset();
        inputStream_ = nullptr;
        outputStream_ = nullptr;
        diagnosticStream_ = nullptr;
        bindIoToShellAndRuntime();
    }

    std::unique_ptr<GeminiSession> GeminiSession::create() {
        return std::make_unique<GeminiSession>();
    }

    void GeminiSession::setGeminiCatalogRoot(std::optional<std::filesystem::path> root) {
        geminiCatalogRoot_ = std::move(root);
    }

    void GeminiSession::attach(const PickCore::UserSession &session) {
        geminiCatalogRoot_ = session.catalogRoot;
        setFileSystemRoot(session.pickRoot);
        reset();
        const int port = daemonPortAssigned_ ? whoPort_ : session.whoPort;
        setSessionIdentity(port, session.username, session.accountName);
        loggedIn_ = true;
        userNo_ = session.userNo.empty() ? std::string{"0"} : session.userNo;
        bindLockSession(makeSessionLockId(port, session.accountName, session.username));
    }

    void GeminiSession::setDaemonPort(const int port) {
        whoPort_ = port;
        daemonPortAssigned_ = true;
    }

    void GeminiSession::setSessionIdentity(const int port, std::string username, std::string account) {
        whoPort_ = port;
        sessionUsername_ = std::move(username);
        sessionAccount_ = std::move(account);
    }

    void GeminiSession::setActiveList(std::vector<std::string> ids, std::string sourceFile) {
        activeList_ = std::move(ids);
        activeListSourceFile_ = std::move(sourceFile);
    }

    void GeminiSession::clearActiveList() {
        activeList_.clear();
        activeListSourceFile_.reset();
    }

    void GeminiSession::reloadMdDefaultDataFile() {
        defaultDataFile_ = PickCore::loadDefaultDataFileFromMd(fileSystem_);
    }

    void GeminiSession::setSharedLockTable(std::shared_ptr<PickCore::Locking::LockTable> table) {
        lockTable_ = std::move(table);
        syncLockContextToFileSystem();
    }

    std::string GeminiSession::makeSessionLockId(const int port,
                                                 const std::string &account,
                                                 const std::string &username) {
        std::ostringstream id;
        id << "S:" << port << ':' << account << ':' << username;
        return id.str();
    }

    void GeminiSession::bindLockSession(const std::string &sessionLockId) {
        sessionLockId_ = sessionLockId;
        syncLockContextToFileSystem();
    }

    void GeminiSession::syncLockContextToFileSystem() {
        if (lockTable_ && !sessionLockId_.empty()) {
            fileSystem_.setLockContext(lockTable_, sessionLockId_);
        } else {
            fileSystem_.clearLockContext();
        }
    }

    void GeminiSession::releaseSessionLocks() {
        if (lockTable_ && !sessionLockId_.empty()) {
            (void) lockTable_->releaseAllForSession(sessionLockId_);
        }
    }

    bool GeminiSession::isSessionSystemVariableName(const std::string_view name) {
        const std::string key = TclEnvironment::canonicalVariableName(name);
        return key == "@USERNO" || key == "@ACCOUNT" || key == "@LOGNAME" || key == "@DEFDATA";
    }

    std::optional<std::string> GeminiSession::resolveSystemVariable(const std::string_view name) const {
        const std::string key = TclEnvironment::canonicalVariableName(name);
        if (key == "@USERNO") {
            return userNo_;
        }
        if (key == "@ACCOUNT") {
            return sessionAccount_;
        }
        if (key == "@LOGNAME") {
            return sessionUsername_;
        }
        if (key == "@DEFDATA") {
            if (defaultDataFile_.has_value()) {
                return *defaultDataFile_;
            }
            return std::string{};
        }
        return std::nullopt;
    }

} // namespace PickShell
