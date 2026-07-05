#include "DefaultFileSystemRoot.h"

#include "HostBootstrap.h"
#include "Shell.h"

void applyHostPathsToShell(PickShell::Shell &shell, const PickCore::DefaultHostPaths &paths) {
    if (paths.pickFilesystemRoot.has_value()) {
        shell.setFileSystemRoot(*paths.pickFilesystemRoot);
    }
    if (paths.geminiCatalogRoot.has_value()) {
        shell.setGeminiCatalogRoot(*paths.geminiCatalogRoot);
    }
}

void applyDefaultFileSystemRoot(PickShell::Shell &shell) {
    applyHostPathsToShell(shell, PickCore::resolveDefaultHostPaths());
}
