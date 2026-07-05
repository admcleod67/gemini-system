#include <ConsoleConfig.h>
#include <DaemonIpcClient.h>

#include <csignal>
#include <iostream>

#ifndef _WIN32
namespace {
    PickCore::DaemonIpcClient *g_activeClient = nullptr;

    void handleTerminationSignal(const int /*signal*/) {
        if (g_activeClient != nullptr) {
            g_activeClient->requestStop();
        }
    }
} // namespace
#endif

int main(const int argc, char *argv[]) {
    const PickCore::ConsoleConfigResolution resolution = PickCore::resolveConsoleConfig(argc, argv);
    if (resolution.showHelp) {
        PickCore::printConsoleUsage(argc > 0 ? argv[0] : "gemini-console");
        return 0;
    }

#ifndef _WIN32
    try {
        PickCore::DaemonIpcClient client(resolution.config.socketPath);
        g_activeClient = &client;
        std::signal(SIGPIPE, SIG_IGN);
        std::signal(SIGINT, handleTerminationSignal);
        std::signal(SIGTERM, handleTerminationSignal);

        client.connect();
        client.handshake();
        client.attachSession(resolution.config.requestedPort);

        const int rc = client.runIoPump(std::cin, std::cout, std::cerr);
        client.disconnect();
        g_activeClient = nullptr;
        return rc;
    } catch (const PickCore::DaemonIpcClientError &error) {
        std::cerr << "gemini-console: " << error.what() << '\n';
        return 1;
    }
#else
    (void) resolution;
    std::cerr << "gemini-console: Unix domain IPC not available on this platform\n";
    return 1;
#endif
}
