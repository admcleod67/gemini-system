#include "ShellSession.h"

#include <algorithm>
#include <vector>

namespace PickShell {
    ShellSession::ShellSession(PickVM::Runtime &runtime)
        : runtime_(runtime), fileSystem_(filesystemRoot_), vocResolver_(fileSystem_) {
    }

    void ShellSession::setProgramsRoot(std::filesystem::path root) {
        programsRoot_ = std::move(root);
    }

    void ShellSession::setFileSystemRoot(std::filesystem::path root) {
        filesystemRoot_ = std::move(root);
        fileSystem_.setRoot(filesystemRoot_);
        vocResolver_.invalidate();
    }

    bool ShellSession::programImageLoaded() const {
        return lastLoaded_.has_value() && runtime_.isLoaded();
    }

    void ShellSession::pruneBreakpointsForProgram(const std::size_t programSize, std::ostream &out) {
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

    void ShellSession::executeVmLoop(std::ostream &out) {
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

    void ShellSession::resetForQuit() {
        lastLoaded_.reset();
        suspended_ = false;
        resumePastBreakpointIp_.reset();
        env_.clear();
        runtime_.loadProgram({});
    }

    void ShellSession::setGeminiCatalogRoot(std::optional<std::filesystem::path> root) {
        geminiCatalogRoot_ = std::move(root);
    }

    void ShellSession::applyUserSession(const PickCore::UserSession &session) {
        geminiCatalogRoot_ = session.catalogRoot;
        setFileSystemRoot(session.pickRoot);
        resetForQuit();
        setSessionIdentity(session.whoPort, session.username, session.accountName);
        loggedIn_ = true;
        (void) env_.set("@USERNO", session.userNo);
        (void) env_.set("@ACCOUNT", session.accountName);
        (void) env_.set("@LOGNAME", session.username);
    }

    void ShellSession::setSessionIdentity(const int port, std::string username, std::string account) {
        whoPort_ = port;
        sessionUsername_ = std::move(username);
        sessionAccount_ = std::move(account);
    }

    void ShellSession::clearLoginSession() {
        loggedIn_ = false;
        whoPort_ = 0;
        sessionUsername_.clear();
        sessionAccount_.clear();
    }

} // namespace PickShell
