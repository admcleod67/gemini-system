#include "BasicCompiler.h"

#include "BasicBytecodeEmitter.h"
#include "BasicSemanticAnalyzer.h"
#include "BasicStatementParser.h"

#include <utility>

namespace PickShell {
    BasicCompileResult BasicCompiler::compile(const BasicProgram &program) const {
        BasicCompileResult result;

        BasicAst::StatementParseResult parsed = BasicStatementParser::parse(program);
        if (!parsed.success) {
            result.success = false;
            result.program.clear();
            result.instructionCount = 0;
            result.labelCount = 0;
            for (const auto &error: parsed.errors) {
                result.errors.push_back({error.line, error.message});
            }
            return result;
        }

        BasicSemanticAnalysisResult semantic = BasicSemanticAnalyzer::analyze(std::move(parsed));
        if (!semantic.success) {
            result.success = false;
            result.program.clear();
            result.instructionCount = 0;
            result.labelCount = 0;
            for (const auto &error: semantic.errors) {
                result.errors.push_back({error.line, error.message});
            }
            return result;
        }

        BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(semantic.program);
        if (!emitted.success) {
            result.success = false;
            result.program.clear();
            result.instructionCount = 0;
            result.labelCount = 0;
            result.errors = std::move(emitted.errors);
            return result;
        }

        result.program = std::move(emitted.program);
        result.instructionCount = result.program.size();
        result.labelCount = 0;
        result.success = true;
        return result;
    }
} // namespace PickShell
