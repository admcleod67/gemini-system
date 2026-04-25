#include "BasicSemanticAnalyzer.h"

#include <unordered_set>
#include <utility>
#include <variant>
#include <type_traits>

namespace PickShell {
    namespace {
        BasicIr::BranchArm toIrBranchArm(BasicAst::BranchArm arm) {
            BasicIr::BranchArm out{};
            out.line = arm.line;
            out.statementText = std::move(arm.statementText);
            return out;
        }

        std::optional<BasicIr::BranchArm> toIrBranchArmOpt(std::optional<BasicAst::BranchArm> arm) {
            if (!arm.has_value()) {
                return std::nullopt;
            }
            return toIrBranchArm(std::move(*arm));
        }
    } // namespace

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
                        if (!stmt.thenArm.line.has_value() ||
                            knownLines.find(*stmt.thenArm.line) == knownLines.end()) {
                            result.errors.push_back({
                                line.lineNumber,
                                "Unknown target line " +
                                        std::to_string(stmt.thenArm.line.value_or(0))
                            });
                        }
                        if (stmt.elseArm.has_value() &&
                            stmt.elseArm->line.has_value() &&
                            knownLines.find(*stmt.elseArm->line) == knownLines.end()) {
                            result.errors.push_back({
                                line.lineNumber,
                                "Unknown target line " + std::to_string(*stmt.elseArm->line)
                            });
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::OpenStmt>) {
                        if (stmt.elseArm.has_value() &&
                            stmt.elseArm->line.has_value() &&
                            knownLines.find(*stmt.elseArm->line) == knownLines.end()) {
                            result.errors.push_back({
                                line.lineNumber,
                                "Unknown target line " + std::to_string(*stmt.elseArm->line)
                            });
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReadStmt>) {
                        if (stmt.elseArm.has_value() &&
                            stmt.elseArm->line.has_value() &&
                            knownLines.find(*stmt.elseArm->line) == knownLines.end()) {
                            result.errors.push_back({
                                line.lineNumber,
                                "Unknown target line " + std::to_string(*stmt.elseArm->line)
                            });
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::WriteStmt>) {
                        if (stmt.elseArm.has_value() &&
                            stmt.elseArm->line.has_value() &&
                            knownLines.find(*stmt.elseArm->line) == knownLines.end()) {
                            result.errors.push_back({
                                line.lineNumber,
                                "Unknown target line " + std::to_string(*stmt.elseArm->line)
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
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ForStmt>) {
                        return BasicIr::ForStmt{
                            std::move(stmt.variableName),
                            std::move(stmt.initExpr),
                            std::move(stmt.limitExpr),
                            std::move(stmt.stepExpr)
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::NextStmt>) {
                        return BasicIr::NextStmt{std::move(stmt.variableName)};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::DimStmt>) {
                        return BasicIr::DimStmt{
                            std::move(stmt.variableName),
                            std::move(stmt.sizeExpr)
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::LetArrayStmt>) {
                        return BasicIr::LetArrayStmt{
                            std::move(stmt.variableName),
                            std::move(stmt.indexExpr),
                            std::move(stmt.valueExpr)
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ClearStmt>) {
                        return BasicIr::ClearStmt{};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::OpenStmt>) {
                        return BasicIr::OpenStmt{
                            std::move(stmt.fileExpr),
                            std::move(stmt.fileVar),
                            toIrBranchArmOpt(std::move(stmt.elseArm))
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReadStmt>) {
                        return BasicIr::ReadStmt{
                            std::move(stmt.targetVar),
                            std::move(stmt.fileVar),
                            std::move(stmt.idExpr),
                            toIrBranchArmOpt(std::move(stmt.elseArm))
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::WriteStmt>) {
                        return BasicIr::WriteStmt{
                            std::move(stmt.valueExpr),
                            std::move(stmt.fileVar),
                            std::move(stmt.idExpr),
                            toIrBranchArmOpt(std::move(stmt.elseArm))
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::CloseStmt>) {
                        return BasicIr::CloseStmt{std::move(stmt.fileVar)};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::IfStmt>) {
                        return BasicIr::IfStmt{
                            std::move(stmt.condition),
                            toIrBranchArm(std::move(stmt.thenArm)),
                            toIrBranchArmOpt(std::move(stmt.elseArm))
                        };
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
