//
// Foreground daemon run loop and graceful shutdown (Milestone 13 Stage 4).
//

#ifndef PICK_SYSTEM_TCL_GEMINI_DAEMON_RUNNER_H
#define PICK_SYSTEM_TCL_GEMINI_DAEMON_RUNNER_H

#include "DaemonConfig.h"
#include "GeminiSessionHost.h"

#ifndef _WIN32
    #include "DaemonIpcServer.h"
#endif

#include <iosfwd>
#include <iostream>
#include <unordered_map>

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
        PickCore::AttachSessionResult attachSession(PickCore::SessionId requestedPort,
                                                    PickCore::IpcSessionChannel &channel);
        void detachSession(PickCore::SessionId port);

        PickCore::GeminiServiceDaemon &daemon_;
        GeminiSessionHost &host_;
        PickCore::DaemonConfig config_;
#ifndef _WIN32
        PickCore::DaemonIpcServer ipcServer_;
        std::unordered_map<PickCore::SessionId, int> boundSessionPorts_;
#endif
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_GEMINI_DAEMON_RUNNER_H
