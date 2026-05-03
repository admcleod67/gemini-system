//
// Cold-start console output before interactive logon (no Tcl).
//

#ifndef PICK_SYSTEM_CORE_BOOT_BOOTMONITOR_H
#define PICK_SYSTEM_CORE_BOOT_BOOTMONITOR_H

#include "HostBootstrap.h"

#include <iosfwd>

namespace PickVM {
    class Runtime;
}

namespace PickCore {
    struct BootContext {
        const PickVM::Runtime *runtime{nullptr};
        DefaultHostPaths hostPaths;
    };

    class BootMonitor {
    public:
        static void runColdStart(std::ostream &out, const BootContext &ctx);
    };
} // namespace PickCore

#endif
