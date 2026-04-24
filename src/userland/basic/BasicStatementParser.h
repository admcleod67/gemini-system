#ifndef PICK_SYSTEM_BASIC_BASICSTATEMENTPARSER_H
#define PICK_SYSTEM_BASIC_BASICSTATEMENTPARSER_H

#pragma once

#include "BasicAst.h"
#include "BasicProgram.h"

namespace PickShell {
    class BasicStatementParser {
    public:
        [[nodiscard]] static BasicAst::StatementParseResult parse(const BasicProgram &program);
    };
} // namespace PickShell

#endif // PICK_SYSTEM_BASIC_BASICSTATEMENTPARSER_H
