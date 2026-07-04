#include <iostream>
#include <optional>

#include <DefaultFileSystemRoot.h>
#include <GeminiServiceDaemon.h>
#include <GeminiSessionHost.h>
#include <LoginService.h>

int main() {
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::createEmbedded();
    PickShell::GeminiSessionHost host(daemon);
    daemon.coldStart(std::cout);

    const PickShell::SessionHandle handle = host.createSession();
    PickShell::GeminiSession &session = handle.session;
    applyDefaultFileSystemRoot(session.shell());

    PickShell::Shell &shell = session.shell();
    const auto &catalog = shell.geminiCatalogRoot();
    if (!catalog.has_value()) {
        host.runExclusive(handle.id, [&] { (void) shell.runTclRepl(); });
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
            return 0;
        }
    }
}
