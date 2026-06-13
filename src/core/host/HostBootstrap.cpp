#include "HostBootstrap.h"

#include <cstdlib>

namespace PickCore {
    namespace {
        void resolveModulesRoot(DefaultHostPaths &out) {
            namespace fs = std::filesystem;

            if (const char *const env = std::getenv("GEMINI_MODULES_PATH")) {
                if (env[0] != '\0') {
                    out.geminiModulesRoot = fs::path(env);
                    return;
                }
            }

            if (out.geminiCatalogRoot.has_value()) {
                out.geminiModulesRoot = *out.geminiCatalogRoot / "modules";
                return;
            }

            const fs::path cwd = fs::current_path();
            const fs::path marker = cwd / "gemini/accounts/SYSPROG/VOC";
            std::error_code ec;
            if (fs::is_directory(marker, ec) && !ec) {
                out.geminiModulesRoot = cwd / "gemini/modules";
            }
        }
    } // namespace

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
                resolveModulesRoot(out);
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
        resolveModulesRoot(out);
        return out;
    }
} // namespace PickCore
