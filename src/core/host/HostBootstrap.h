//
// Resolve default host paths for Pick root and Gemini catalogue (no userland types).
//

#ifndef PICK_SYSTEM_CORE_HOST_HOSTBOOTSTRAP_H
#define PICK_SYSTEM_CORE_HOST_HOSTBOOTSTRAP_H

#include <filesystem>
#include <optional>

namespace PickCore {
    struct DefaultHostPaths {
        std::optional<std::filesystem::path> pickFilesystemRoot;
        std::optional<std::filesystem::path> geminiCatalogRoot;
    };

    /// Mirrors prior `applyDefaultFileSystemRoot` path rules without touching `Shell`.
    [[nodiscard]] DefaultHostPaths resolveDefaultHostPaths();
} // namespace PickCore

#endif
