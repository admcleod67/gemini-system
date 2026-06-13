#include <iostream>
#include <optional>

#include <pickvm/core.hpp>
#include <BootMonitor.h>
#include <DefaultFileSystemRoot.h>
#include <HostBootstrap.h>
#include <LanguageRegistry.h>
#include <LoginService.h>
#include <Shell.h>

int main() {
    PickVM::Runtime vm;
    PickShell::Shell shell(vm);
    applyDefaultFileSystemRoot(shell);

    PickCore::BootContext bootCtx;
    bootCtx.runtime = &vm;
    bootCtx.hostPaths = PickCore::resolveDefaultHostPaths();
    PickCore::BootMonitor::runColdStart(std::cout, bootCtx);
    vm.setLanguageRegistry(&PickCore::Languages::LanguageRegistry::instance());

    const auto &catalog = shell.geminiCatalogRoot();
    if (!catalog.has_value()) {
        (void) shell.runTclRepl();
        return 0;
    }

    PickCore::CatalogLoginPhase loginPhase = PickCore::CatalogLoginPhase::ColdStartPortInit;
    for (;;) {
        std::optional<PickCore::UserSession> session = PickCore::LoginService::runCatalogLogin(
            std::cin, std::cout, *catalog, shell.fileSystemRoot(), &std::cerr, loginPhase);
        loginPhase = PickCore::CatalogLoginPhase::InteractiveOnly;
        if (!session.has_value()) {
            return 0;
        }
        shell.attachUserSession(*session);
        const PickShell::ShellRunResult r = shell.runTclRepl();
        if (r == PickShell::ShellRunResult::ExitProcess) {
            return 0;
        }
    }
}
