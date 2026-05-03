#include "DefaultFileSystemRoot.h"

#include "HostBootstrap.h"
#include "Shell.h"

void applyDefaultFileSystemRoot(PickShell::Shell &shell) {
    const PickCore::DefaultHostPaths paths = PickCore::resolveDefaultHostPaths();
    if (paths.pickFilesystemRoot.has_value()) {
        shell.setFileSystemRoot(*paths.pickFilesystemRoot);
    }
    if (paths.geminiCatalogRoot.has_value()) {
        shell.setGeminiCatalogRoot(*paths.geminiCatalogRoot);
    }
}
