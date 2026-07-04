#include <iostream>
#include <optional>

#include <BootMonitor.h>
#include <DefaultFileSystemRoot.h>
#include <GeminiSession.h>
#include <HostBootstrap.h>
#include <LanguageRegistry.h>
#include <LoginService.h>

int main() {
    PickShell::GeminiSession session;
    applyDefaultFileSystemRoot(session.shell());

    PickCore::BootContext bootCtx;
    bootCtx.runtime = &session.runtime();
    bootCtx.hostPaths = PickCore::resolveDefaultHostPaths();
    PickCore::BootMonitor::runColdStart(std::cout, bootCtx);
    session.runtime().setLanguageRegistry(&PickCore::Languages::LanguageRegistry::instance());

    PickShell::Shell &shell = session.shell();
    const auto &catalog = shell.geminiCatalogRoot();
    if (!catalog.has_value()) {
        (void) shell.runTclRepl();
        return 0;
    }

    PickCore::CatalogLoginPhase loginPhase = PickCore::CatalogLoginPhase::ColdStartPortInit;
    for (;;) {
        std::optional<PickCore::UserSession> userSession = PickCore::LoginService::runCatalogLogin(
            std::cin, std::cout, *catalog, shell.fileSystemRoot(), &std::cerr, loginPhase);
        loginPhase = PickCore::CatalogLoginPhase::InteractiveOnly;
        if (!userSession.has_value()) {
            return 0;
        }
        shell.attachUserSession(*userSession);
        const PickShell::ShellRunResult r = shell.runTclRepl();
        if (r == PickShell::ShellRunResult::ExitProcess) {
            return 0;
        }
    }
}
