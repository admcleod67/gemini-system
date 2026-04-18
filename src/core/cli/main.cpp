//
// Created by Allan McLeod on 18/04/2026.
//

#include "../vm/VM.h"
#include "../../tcl/tcl.h"

int main() {
    PickVM::VM vm;
    PickTCL::repl(vm);
    return 0;
}
