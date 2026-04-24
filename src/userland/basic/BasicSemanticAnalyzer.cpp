#include "BasicSemanticAnalyzer.h"

#include <unordered_set>
#include <utility>
#include <variant>

namespace PickShell {
    BasicAst::SemanticResult BasicSemanticAnalyzer::analyze(BasicAst::StatementParseResult parsed) {
        BasicAst::SemanticResult result;
        if (!parsed.success) {
            result.success = false;
            for (const auto &error: parsed.errors) {
                result.errors.push_back({error.line, error.message});
            }
            return result;
        }

        std::unordered_set<int> knownLines;
        knownLines.reserve(parsed.lines.size());
        for (const BasicAst::ParsedBasicLine &line: parsed.lines) {
            knownLines.insert(line.lineNumber);
        }

        for (const BasicAst::ParsedBasicLine &line: parsed.lines) {
            std::visit(
                [&](const auto &stmt) {
                    using StmtT = std::decay_t<decltype(stmt)>;
                    if constexpr (std::is_same_v<StmtT, BasicAst::GotoStmt>) {
                        if (knownLines.find(stmt.targetLine) == knownLines.end()) {
                            result.errors.push_back({
                                line.lineNumber,
                                "Unknown target line " + std::to_string(stmt.targetLine)
                            });
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::IfStmt>) {
                        if (knownLines.find(stmt.thenLine) == knownLines.end()) {
                            result.errors.push_back({
                                line.lineNumber,
                                "Unknown target line " + std::to_string(stmt.thenLine)
                            });
                        }
                        if (stmt.elseLine.has_value() &&
                            knownLines.find(*stmt.elseLine) == knownLines.end()) {
                            result.errors.push_back({
                                line.lineNumber,
                                "Unknown target line " + std::to_string(*stmt.elseLine)
                            });
                        }
                    }
                },
                line.statement);
        }

        if (!result.errors.empty()) {
            result.success = false;
            return result;
        }

        result.success = true;
        result.lines.reserve(parsed.lines.size());
        for (BasicAst::ParsedBasicLine &line: parsed.lines) {
            result.lines.push_back({line.lineNumber, std::move(line.statement)});
        }
        return result;
    }
} // namespace PickShell
