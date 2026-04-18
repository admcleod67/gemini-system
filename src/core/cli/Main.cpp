//
// Created by Allan McLeod on 18/04/2026.
//

#include <pickvm/core.hpp>
#include <Shell.h>

int main() {
    PickVM::Runtime vm;
    PickShell::Shell shell(vm);
    shell.run();
    return 0;
}
