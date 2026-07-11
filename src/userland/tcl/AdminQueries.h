//
// Admin LISTSESSIONS / STATUS DTOs and Shell query callbacks (Milestone 17 Stage 4).
//

#ifndef PICK_SYSTEM_TCL_ADMIN_QUERIES_H
#define PICK_SYSTEM_TCL_ADMIN_QUERIES_H

#include <CooperativeSessionRunner.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace PickShell {
    struct AdminSessionRow {
        PickCore::SessionId port{0};
        bool consoleBound{false};
        bool loggedIn{false};
        std::string username;
        std::string account;
        PickCore::SessionRunState runState{PickCore::SessionRunState::Runnable};
    };

    struct AdminDaemonStatus {
        std::size_t maxSessions{0};
        std::size_t sessionCount{0};
        std::filesystem::path socketPath;
        std::string version;
    };

    struct ShellAdminQueries {
        std::function<std::vector<AdminSessionRow>()> listSessions;
        std::function<AdminDaemonStatus()> status;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_ADMIN_QUERIES_H
