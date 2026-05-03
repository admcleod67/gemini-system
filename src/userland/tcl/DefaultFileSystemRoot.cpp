#include "DefaultFileSystemRoot.h"

#include "Shell.h"

#include <cstdlib>
#include <filesystem>

void applyDefaultFileSystemRoot(PickShell::Shell &shell) {
    namespace fs = std::filesystem;
    if (const char *const env = std::getenv("GEMINI_FILESYSTEM_ROOT")) {
        if (env[0] != '\0') {
            shell.setFileSystemRoot(fs::path(env));
            if (const char *const cat = std::getenv("GEMINI_CATALOG_ROOT")) {
                if (cat[0] != '\0') {
                    shell.setGeminiCatalogRoot(fs::path(cat));
                }
            }
            return;
        }
    }

    const fs::path cwd = fs::current_path();
    const fs::path marker = cwd / "gemini/accounts/SYSPROG/VOC";
    std::error_code ec;
    if (fs::is_directory(marker, ec) && !ec) {
        shell.setFileSystemRoot(cwd / "gemini/accounts/SYSPROG");
        shell.setGeminiCatalogRoot(cwd / "gemini");
    }
}
