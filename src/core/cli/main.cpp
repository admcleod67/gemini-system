//
// Created by Allan McLeod on 18/04/2026.
//

#include "../vm/Runtime.h"
#include "../../tcl/Shell.h"

int main() {
    PickVM::Runtime vm;
    PickShell::Shell shell(vm);
    shell.run();
    return 0;
}
