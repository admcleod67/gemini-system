//
// Immutable handoff from core login to userland (Tcl shell).
//

#ifndef PICK_SYSTEM_CORE_LOGIN_USERSESSION_H
#define PICK_SYSTEM_CORE_LOGIN_USERSESSION_H

#include <filesystem>
#include <string>

namespace PickCore {
    struct UserSession {
        std::filesystem::path catalogRoot;
        std::filesystem::path pickRoot;
        std::string username;
        std::string accountName;
        int whoPort{0};
        std::string userNo{"0"};
    };
} // namespace PickCore

#endif
