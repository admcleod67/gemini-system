#ifndef PICK_SYSTEM_BASIC_BASICSEMANTICANALYZER_H
#define PICK_SYSTEM_BASIC_BASICSEMANTICANALYZER_H

#pragma once

#include "BasicAst.h"
#include "BasicNormalizedIr.h"

#include <vector>

namespace PickShell {
    struct BasicSemanticAnalysisResult {
        bool success{false};
        BasicIr::NormalizedProgram program;
        std::vector<BasicAst::SemanticError> errors;
    };

    class BasicSemanticAnalyzer {
    public:
        [[nodiscard]] static BasicSemanticAnalysisResult analyze(BasicAst::StatementParseResult parsed);
    };
} // namespace PickShell

#endif // PICK_SYSTEM_BASIC_BASICSEMANTICANALYZER_H
