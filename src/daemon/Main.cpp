#include <cstdio>
#include <iostream>

#include <DaemonConfig.h>
#include <GeminiDaemonRunner.h>
#include <GeminiServiceDaemon.h>
#include <GeminiSessionHost.h>

int main(const int argc, char *argv[]) {
    const PickCore::DaemonConfigResolution resolution = PickCore::resolveDaemonConfig(argc, argv);
    if (resolution.showHelp) {
        PickCore::printDaemonUsage(argc > 0 ? argv[0] : "gemini-daemon");
        return 0;
    }
    if (resolution.error.has_value()) {
        std::cerr << "gemini-daemon: " << *resolution.error << '\n';
        return 1;
    }

#ifndef _WIN32
    // Line-buffer stdout / unbuffer stderr so boot and listen lines reach journald promptly.
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(resolution.config);
    PickShell::GeminiSessionHost host(daemon, resolution.config.maxSessions);
    PickShell::GeminiDaemonRunner runner(daemon, host, resolution.config);
    return runner.run();
}
