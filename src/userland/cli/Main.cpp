//
// Created by Allan McLeod on 18/04/2026.
//

#include <iostream>
#include <pickvm/core.hpp>
#include <DefaultFileSystemRoot.h>
#include <Shell.h>

int main() {
    PickVM::Runtime vm;
    PickShell::Shell shell(vm);
    applyDefaultFileSystemRoot(shell);
    shell.tryAutoLoginFromEnvironment(std::cerr);
    shell.run();
    return 0;
}
