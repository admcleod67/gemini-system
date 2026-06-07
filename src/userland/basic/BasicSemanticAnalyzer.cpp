#include "BasicSemanticAnalyzer.h"

#include <unordered_set>
#include <utility>
#include <variant>
#include <type_traits>

namespace PickShell {
    namespace {
        BasicIr::NormalizedStmt toIrStatement(BasicAst::StatementNode statement);

        BasicIr::BranchArm toIrBranchArm(BasicAst::BranchArm arm) {
            BasicIr::BranchArm out{};
            out.line = arm.line;
            if (arm.inlineStatement) {
                auto inlineStmt = std::make_shared<BasicIr::InlineStatement>();
                inlineStmt->statement = toIrStatement(std::move(arm.inlineStatement->statement));
                out.inlineStatement = std::move(inlineStmt);
            }
            return out;
        }

        std::optional<BasicIr::BranchArm> toIrBranchArmOpt(std::optional<BasicAst::BranchArm> arm) {
            if (!arm.has_value()) {
                return std::nullopt;
            }
            return toIrBranchArm(std::move(*arm));
        }

        void addUnknownTargetError(std::vector<BasicAst::SemanticError> &errors, int ownerLine, int targetLine) {
            errors.push_back({
                ownerLine,
                "Unknown target line " + std::to_string(targetLine)
            });
        }

        bool validateLineTarget(const std::unordered_set<int> &knownLines,
                                int ownerLine,
                                int targetLine,
                                std::vector<BasicAst::SemanticError> &errors) {
            if (knownLines.find(targetLine) != knownLines.end()) {
                return true;
            }
            addUnknownTargetError(errors, ownerLine, targetLine);
            return false;
        }

        void validateStatement(const BasicAst::StatementNode &statement,
                               int ownerLine,
                               const std::unordered_set<int> &knownLines,
                               std::vector<BasicAst::SemanticError> &errors);

        void validateBranchArm(const BasicAst::BranchArm &arm,
                               int ownerLine,
                               const std::unordered_set<int> &knownLines,
                               std::vector<BasicAst::SemanticError> &errors) {
            if (arm.line.has_value()) {
                (void) validateLineTarget(knownLines, ownerLine, *arm.line, errors);
                return;
            }

            if (!arm.inlineStatement) {
                errors.push_back({ownerLine, "Branch arm requires a line target or statement"});
                return;
            }
            validateStatement(arm.inlineStatement->statement, ownerLine, knownLines, errors);
        }

        void validateStatement(const BasicAst::StatementNode &statement,
                               int ownerLine,
                               const std::unordered_set<int> &knownLines,
                               std::vector<BasicAst::SemanticError> &errors) {
            std::visit(
                [&](const auto &stmt) {
                    using StmtT = std::decay_t<decltype(stmt)>;
                    if constexpr (std::is_same_v<StmtT, BasicAst::GotoStmt>) {
                        (void) validateLineTarget(knownLines, ownerLine, stmt.targetLine, errors);
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::GosubStmt>) {
                        (void) validateLineTarget(knownLines, ownerLine, stmt.targetLine, errors);
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::IfStmt>) {
                        validateBranchArm(stmt.thenArm, ownerLine, knownLines, errors);
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::OpenStmt>) {
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReadStmt>) {
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::WriteStmt>) {
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReadNextStmt>) {
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReadVStmt>) {
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::WriteVStmt>) {
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReadUStmt>) {
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::WriteUStmt>) {
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::OnErrorStmt>) {
                        if (!stmt.stop) {
                            (void) validateLineTarget(knownLines, ownerLine, stmt.targetLine, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::MatReadStmt>) {
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::MatWriteStmt>) {
                        if (stmt.elseArm.has_value()) {
                            validateBranchArm(*stmt.elseArm, ownerLine, knownLines, errors);
                        }
                    }
                },
                statement);
        }

        BasicIr::NormalizedStmt toIrStatement(BasicAst::StatementNode statement) {
            return std::visit(
                [](auto &stmt) -> BasicIr::NormalizedStmt {
                    using StmtT = std::decay_t<decltype(stmt)>;
                    if constexpr (std::is_same_v<StmtT, BasicAst::LetStmt>) {
                        return BasicIr::LetStmt{std::move(stmt.variableName), std::move(stmt.expression)};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::InputStmt>) {
                        return BasicIr::InputStmt{std::move(stmt.promptExpr), std::move(stmt.variableName)};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ChainStmt>) {
                        return BasicIr::ChainStmt{std::move(stmt.programExpr)};
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
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReadUStmt>) {
                        return BasicIr::ReadUStmt{
                            std::move(stmt.targetVar),
                            std::move(stmt.fileVar),
                            std::move(stmt.idExpr),
                            toIrBranchArmOpt(std::move(stmt.elseArm))
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::WriteUStmt>) {
                        return BasicIr::WriteUStmt{
                            std::move(stmt.valueExpr),
                            std::move(stmt.fileVar),
                            std::move(stmt.idExpr),
                            toIrBranchArmOpt(std::move(stmt.elseArm))
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReleaseStmt>) {
                        return BasicIr::ReleaseStmt{
                            std::move(stmt.fileVar),
                            std::move(stmt.idExpr)
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::OnErrorStmt>) {
                        return BasicIr::OnErrorStmt{stmt.stop, stmt.targetLine};
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReadNextStmt>) {
                        return BasicIr::ReadNextStmt{
                            std::move(stmt.targetVar),
                            std::move(stmt.fileVar),
                            toIrBranchArmOpt(std::move(stmt.elseArm))
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::ReadVStmt>) {
                        return BasicIr::ReadVStmt{
                            std::move(stmt.targetVar),
                            std::move(stmt.fileVar),
                            std::move(stmt.idExpr),
                            std::move(stmt.attrExpr),
                            std::move(stmt.valueIndexExpr),
                            toIrBranchArmOpt(std::move(stmt.elseArm))
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::WriteVStmt>) {
                        return BasicIr::WriteVStmt{
                            std::move(stmt.valueExpr),
                            std::move(stmt.fileVar),
                            std::move(stmt.idExpr),
                            std::move(stmt.attrExpr),
                            std::move(stmt.valueIndexExpr),
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
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::MatAssignStmt>) {
                        return BasicIr::MatAssignStmt{
                            std::move(stmt.targetArray),
                            std::move(stmt.rhsExpr),
                            std::move(stmt.rhsSourceArray)
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::MatReadStmt>) {
                        return BasicIr::MatReadStmt{
                            std::move(stmt.targetArray),
                            std::move(stmt.fileVar),
                            std::move(stmt.idExpr),
                            toIrBranchArmOpt(std::move(stmt.elseArm))
                        };
                    } else if constexpr (std::is_same_v<StmtT, BasicAst::MatWriteStmt>) {
                        return BasicIr::MatWriteStmt{
                            std::move(stmt.sourceArray),
                            std::move(stmt.fileVar),
                            std::move(stmt.idExpr),
                            toIrBranchArmOpt(std::move(stmt.elseArm))
                        };
                    } else {
                        return BasicIr::EndStmt{};
                    }
                },
                statement);
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
            validateStatement(line.statement, line.lineNumber, knownLines, result.errors);
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
            normalized.statement = toIrStatement(std::move(line.statement));
            result.program.lines.push_back(std::move(normalized));
        }
        return result;
    }
} // namespace PickShell
