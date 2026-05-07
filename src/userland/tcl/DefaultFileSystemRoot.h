//
// Resolve default Pick filesystem root for gemini-system.
//

#ifndef PICK_SYSTEM_TCL_DEFAULTFILESYSTEMROOT_H
#define PICK_SYSTEM_TCL_DEFAULTFILESYSTEMROOT_H

namespace PickShell {
    class Shell;
}

void applyDefaultFileSystemRoot(PickShell::Shell &shell);

#endif
