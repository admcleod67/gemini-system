#include <atomic>
#include <iostream>
#include <optional>

#include <DefaultFileSystemRoot.h>
#include <GeminiServiceDaemon.h>
#include <GeminiSessionHost.h>
#include <LoginService.h>
#include <Shell.h>

int main() {
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::createEmbedded();
    PickShell::GeminiSessionHost host(daemon);
    daemon.coldStart(std::cout);

    const PickShell::SessionHandle handle = host.createSession();
    PickShell::GeminiSession &session = handle.session;
    applyDefaultFileSystemRoot(session.shell());

    std::atomic<bool> shutdownRequested{false};
    PickShell::Shell &shell = session.shell();
    shell.setAdminQueries(PickShell::ShellAdminQueries{
        .listSessions = [&host] { return host.listAdminSessions(); },
        .status = [&host] { return host.adminStatus(); },
        .killSession =
            [&host](const PickCore::SessionId port, std::string &error) {
                if (host.sessions().find(port) == nullptr) {
                    error = "session not found";
                    return false;
                }
                host.retireSession(port);
                host.destroySession(port);
                return true;
            },
        .requestShutdown = [&shutdownRequested] { shutdownRequested.store(true, std::memory_order_release); },
    });
    const auto &catalog = shell.geminiCatalogRoot();
    if (!catalog.has_value()) {
        host.runExclusive(handle.id, [&] { (void) shell.runTclRepl(); });
        if (shutdownRequested.load(std::memory_order_acquire)) {
            host.destroyAllSessions();
        }
        return 0;
    }

    PickCore::CatalogLoginPhase loginPhase = PickCore::CatalogLoginPhase::ColdStartPortInit;
    for (;;) {
        std::optional<PickCore::UserSession> userSession = PickCore::LoginService::runCatalogLogin(
            session.input(), session.output(), *catalog, shell.fileSystemRoot(), &session.diagnostic(), loginPhase);
        loginPhase = PickCore::CatalogLoginPhase::InteractiveOnly;
        if (!userSession.has_value()) {
            return 0;
        }
        session.attach(*userSession);
        const PickShell::ShellRunResult r = [&] {
            PickShell::ShellRunResult result = PickShell::ShellRunResult::ExitProcess;
            host.runExclusive(handle.id, [&] { result = shell.runTclRepl(); });
            return result;
        }();
        if (r == PickShell::ShellRunResult::ExitProcess) {
            if (shutdownRequested.load(std::memory_order_acquire)) {
                host.destroyAllSessions();
            }
            return 0;
        }
    }
}
