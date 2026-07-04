//
// Foreground daemon run loop and graceful shutdown (Milestone 13 Stage 4).
//

#ifndef PICK_SYSTEM_TCL_GEMINI_DAEMON_RUNNER_H
#define PICK_SYSTEM_TCL_GEMINI_DAEMON_RUNNER_H

#include "DaemonConfig.h"
#include "GeminiSessionHost.h"

#include <atomic>
#include <iosfwd>
#include <iostream>

namespace PickShell {
    class GeminiDaemonRunner {
    public:
        GeminiDaemonRunner(PickCore::GeminiServiceDaemon &daemon,
                           GeminiSessionHost &host,
                           const PickCore::DaemonConfig &config);

        int run(std::ostream &bootOut = std::cout);
        void requestShutdown();

    private:
        void installSignalHandlers();
        void shutdown();

        PickCore::GeminiServiceDaemon &daemon_;
        GeminiSessionHost &host_;
        PickCore::DaemonConfig config_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_GEMINI_DAEMON_RUNNER_H
