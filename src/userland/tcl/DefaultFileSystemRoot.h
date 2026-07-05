//
// Resolve default Pick filesystem root for gemini-system.
//

#ifndef PICK_SYSTEM_TCL_DEFAULTFILESYSTEMROOT_H
#define PICK_SYSTEM_TCL_DEFAULTFILESYSTEMROOT_H

#include "HostBootstrap.h"

namespace PickShell {
    class Shell;
}

void applyHostPathsToShell(PickShell::Shell &shell, const PickCore::DefaultHostPaths &paths);
void applyDefaultFileSystemRoot(PickShell::Shell &shell);

#endif
