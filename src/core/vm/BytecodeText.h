#ifndef PICK_SYSTEM_VM_BYTECODETEXT_H
#define PICK_SYSTEM_VM_BYTECODETEXT_H

#include "Runtime.h"

#include <string>
#include <vector>

namespace PickVM {
    std::string serializeBytecodeText(const std::vector<Instruction> &program,
                                      const std::vector<int> &sourceLinePerInstr);
}

#endif // PICK_SYSTEM_VM_BYTECODETEXT_H
