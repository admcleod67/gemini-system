#include <iostream>
#include <memory>
#include <optional>

#include <DefaultFileSystemRoot.h>
#include <GeminiServiceDaemon.h>
#include <GeminiSession.h>
#include <LoginService.h>

int main() {
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::createEmbedded();
    const auto session = PickShell::GeminiSession::create();
    applyDefaultFileSystemRoot(session->shell());

    daemon.coldStart(std::cout);
    session->runtime().setLanguageRegistry(&daemon.languageRegistry());

    PickShell::Shell &shell = session->shell();
    const auto &catalog = shell.geminiCatalogRoot();
    if (!catalog.has_value()) {
        (void) shell.runTclRepl();
        return 0;
    }

    PickCore::CatalogLoginPhase loginPhase = PickCore::CatalogLoginPhase::ColdStartPortInit;
    for (;;) {
        std::optional<PickCore::UserSession> userSession = PickCore::LoginService::runCatalogLogin(
            session->input(), session->output(), *catalog, shell.fileSystemRoot(), &session->diagnostic(), loginPhase);
        loginPhase = PickCore::CatalogLoginPhase::InteractiveOnly;
        if (!userSession.has_value()) {
            return 0;
        }
        session->attach(*userSession);
        const PickShell::ShellRunResult r = shell.runTclRepl();
        if (r == PickShell::ShellRunResult::ExitProcess) {
            return 0;
        }
    }
}
