#ifndef PICK_SYSTEM_BASIC_BASICSEMANTICANALYZER_H
#define PICK_SYSTEM_BASIC_BASICSEMANTICANALYZER_H

#pragma once

#include "BasicAst.h"

namespace PickShell {
    class BasicSemanticAnalyzer {
    public:
        [[nodiscard]] static BasicAst::SemanticResult analyze(BasicAst::StatementParseResult parsed);
    };
} // namespace PickShell

#endif // PICK_SYSTEM_BASIC_BASICSEMANTICANALYZER_H
