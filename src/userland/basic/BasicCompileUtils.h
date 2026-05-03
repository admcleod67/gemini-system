#ifndef PICK_SYSTEM_BASIC_BASICCOMPILEUTILS_H
#define PICK_SYSTEM_BASIC_BASICCOMPILEUTILS_H

#pragma once

#include <string>

namespace PickShell {
    [[nodiscard]] std::string uppercase(std::string s);

    [[nodiscard]] bool isValidVariableName(const std::string &token);

    [[nodiscard]] bool isReadonlySessionSystemVariable(const std::string &token);
}

#endif // PICK_SYSTEM_BASIC_BASICCOMPILEUTILS_H
