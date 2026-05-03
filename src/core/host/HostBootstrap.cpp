#include "HostBootstrap.h"

#include <cstdlib>

namespace PickCore {
    DefaultHostPaths resolveDefaultHostPaths() {
        namespace fs = std::filesystem;
        DefaultHostPaths out;

        if (const char *const env = std::getenv("GEMINI_FILESYSTEM_ROOT")) {
            if (env[0] != '\0') {
                out.pickFilesystemRoot = fs::path(env);
                if (const char *const cat = std::getenv("GEMINI_CATALOG_ROOT")) {
                    if (cat[0] != '\0') {
                        out.geminiCatalogRoot = fs::path(cat);
                    }
                }
                return out;
            }
        }

        const fs::path cwd = fs::current_path();
        const fs::path marker = cwd / "gemini/accounts/SYSPROG/VOC";
        std::error_code ec;
        if (fs::is_directory(marker, ec) && !ec) {
            out.pickFilesystemRoot = cwd / "gemini/accounts/SYSPROG";
            out.geminiCatalogRoot = cwd / "gemini";
        }
        return out;
    }
} // namespace PickCore
