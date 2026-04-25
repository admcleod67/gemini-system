#include "BasicSemanticAnalyzer.h"

#include <unordered_set>
#include <utility>
#include <variant>
#include <type_traits>

namespace PickShell {
    BasicSemanticAnalysisResult BasicSemanticAnalyzer::analyze(BasicAst::StatementParseResult parsed) {
        BasicSemanticAnalysisResult result;
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
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::GosubStmt>) {
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
        result.program.lines.reserve(parsed.lines.size());
        for (BasicAst::ParsedBasicLine &line: parsed.lines) {
            BasicIr::NormalizedLine normalized;
            normalized.lineNumber = line.lineNumber;
            normalized.statement = std::visit(
                [](auto &stmt) -> BasicIr::NormalizedStmt {
                    using StmtT = std::decay_t<decltype(stmt)>;
                    if constexpr (std::is_same_v<StmtT, BasicAst::LetStmt>) {
                        return BasicIr::LetStmt{std::move(stmt.variableName), std::move(stmt.expression)};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::InputStmt>) {
                        return BasicIr::InputStmt{std::move(stmt.variableName)};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::GotoStmt>) {
                        return BasicIr::GotoStmt{stmt.targetLine};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::GosubStmt>) {
                        return BasicIr::GosubStmt{stmt.targetLine};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReturnStmt>) {
                        return BasicIr::ReturnStmt{};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::IfStmt>) {
                        return BasicIr::IfStmt{std::move(stmt.condition), stmt.thenLine, stmt.elseLine};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::PrintStmt>) {
                        return BasicIr::PrintStmt{std::move(stmt.expression), stmt.suppressEol};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::RemStmt>) {
                        return BasicIr::RemStmt{};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::StopStmt>) {
                        return BasicIr::StopStmt{};
                    } else {
                        return BasicIr::EndStmt{};
                    }
                },
                line.statement);
            result.program.lines.push_back(std::move(normalized));
        }
        return result;
    }
} // namespace PickShell
