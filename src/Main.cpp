#include <cstdio>
#include <iostream>

#include <pick_system/version.hpp>
#include <pickvm/core.hpp>
#include <DefaultFileSystemRoot.h>
#include <LoginService.h>
#include <Shell.h>

int main() {
    std::printf("%s %s\n", pick_system::system_title, pick_system::version_string);
    PickVM::Runtime vm;
    PickShell::Shell shell(vm);
    applyDefaultFileSystemRoot(shell);

    const auto &catalog = shell.geminiCatalogRoot();
    if (!catalog.has_value()) {
        (void) shell.runTclRepl();
        return 0;
    }

    for (;;) {
        std::optional<PickCore::UserSession> session = PickCore::LoginService::tryAutoLoginFromEnv(*catalog, &std::cerr);
        if (!session.has_value()) {
            session = PickCore::LoginService::runConsoleLogin(std::cin, std::cout, *catalog);
        }
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
