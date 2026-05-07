#include "BasicStatementParser.h"

#include "BasicCompileUtils.h"
#include "BasicExpressionParser.h"

#include <optional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace PickShell {
    namespace {
        std::string trim(const std::string &raw) {
            const std::size_t first = raw.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return "";
            }
            const std::size_t last = raw.find_last_not_of(" \t\r\n");
            return raw.substr(first, last - first + 1);
        }

        bool parsePositiveLineNumber(const std::string &raw, int &lineNumber) {
            const std::string token = trim(raw);
            if (token.empty()) {
                return false;
            }
            for (const char c: token) {
                if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
                    return false;
                }
            }
            try {
                lineNumber = std::stoi(token);
            } catch (const std::exception &) {
                return false;
            }
            return lineNumber > 0;
        }

        std::size_t findKeywordToken(const std::string &upperText, const std::string &keyword) {
            std::size_t pos = upperText.find(keyword);
            while (pos != std::string::npos) {
                const bool startOk = pos == 0 || std::isspace(static_cast<unsigned char>(upperText[pos - 1])) != 0;
                const std::size_t endPos = pos + keyword.size();
                const bool endOk = endPos >= upperText.size() ||
                                   std::isspace(static_cast<unsigned char>(upperText[endPos])) != 0;
                if (startOk && endOk) {
                    return pos;
                }
                pos = upperText.find(keyword, pos + 1);
            }
            return std::string::npos;
        }

        bool parseBranchArmSpec(const std::string &raw,
                                const std::string &context,
                                BasicAst::BranchArm &outArm,
                                std::string &error) {
            const std::string token = trim(raw);
            if (token.empty()) {
                error = context + " requires a line number or statement";
                return false;
            }

            int lineNumber = 0;
            if (parsePositiveLineNumber(token, lineNumber)) {
                outArm = BasicAst::BranchArm{lineNumber, nullptr};
                return true;
            }

            if (token.find(':') != std::string::npos) {
                error = context + " supports exactly one statement";
                return false;
            }

            std::istringstream branchIss(token);
            std::string branchKeyword;
            branchIss >> branchKeyword;
            const std::string branchOp = uppercase(branchKeyword);
            if (branchOp == "FOR" || branchOp == "NEXT") {
                error = context + " does not allow FOR/NEXT";
                return false;
            }

            BasicProgram synthetic;
            synthetic.setLine(1, token);
            BasicAst::StatementParseResult parsed = BasicStatementParser::parse(synthetic);
            if (!parsed.success) {
                error = parsed.errors.empty() ? context + " statement parse failed" : parsed.errors.front().message;
                return false;
            }
            if (parsed.lines.size() != 1) {
                error = context + " supports exactly one statement";
                return false;
            }

            auto inlineStmt = std::make_shared<BasicAst::InlineStatement>();
            inlineStmt->statement = std::move(parsed.lines[0].statement);
            outArm = BasicAst::BranchArm{std::nullopt, std::move(inlineStmt)};
            return true;
        }
    } // namespace

    BasicAst::StatementParseResult BasicStatementParser::parse(const BasicProgram &program) {
        BasicAst::StatementParseResult result;

        for (const auto &[lineNumber, sourceText]: program.lines()) {
            const std::string line = trim(sourceText);
            if (line.empty()) {
                continue;
            }

            std::istringstream iss(line);
            std::string keyword;
            iss >> keyword;
            const std::string op = uppercase(keyword);

            if (op == "REM") {
                result.lines.push_back({lineNumber, BasicAst::RemStmt{}});
                continue;
            }

            if (op == "LET") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                const std::size_t eqPos = rest.find('=');
                if (eqPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "LET requires assignment with '='"});
                    continue;
                }
                const std::string lhsRaw = trim(rest.substr(0, eqPos));
                const std::string rhsRaw = trim(rest.substr(eqPos + 1));
                if (rhsRaw.empty()) {
                    result.errors.push_back({lineNumber, "LET requires an expression"});
                    continue;
                }

                // Detect array subscript: base(index) = value
                const std::size_t lparenPos = lhsRaw.find('(');
                if (lparenPos != std::string::npos) {
                    const std::string baseName = trim(lhsRaw.substr(0, lparenPos));
                    if (!isValidVariableName(baseName)) {
                        result.errors.push_back({lineNumber, "Invalid array variable name '" + baseName + "'"});
                        continue;
                    }
                    if (isReadonlySessionSystemVariable(baseName)) {
                        result.errors.push_back({lineNumber, "Read-only system variable '" + baseName + "'"});
                        continue;
                    }
                    const std::size_t rparenPos = lhsRaw.rfind(')');
                    if (rparenPos == std::string::npos || rparenPos <= lparenPos) {
                        result.errors.push_back({lineNumber, "Missing ')' in array subscript"});
                        continue;
                    }
                    const std::string idxRaw = trim(lhsRaw.substr(lparenPos + 1, rparenPos - lparenPos - 1));
                    if (idxRaw.empty()) {
                        result.errors.push_back({lineNumber, "Array subscript cannot be empty"});
                        continue;
                    }
                    BasicExpressionParseResult idxExpr = BasicExpressionParser::parse(idxRaw);
                    if (!idxExpr.success || !idxExpr.expression) {
                        result.errors.push_back({lineNumber, "Array index error: " + idxExpr.error});
                        continue;
                    }
                    BasicExpressionParseResult valExpr = BasicExpressionParser::parse(rhsRaw);
                    if (!valExpr.success || !valExpr.expression) {
                        result.errors.push_back({lineNumber, "LET expression error: " + valExpr.error});
                        continue;
                    }
                    BasicAst::LetArrayStmt stmt{};
                    stmt.variableName = baseName;
                    stmt.indexExpr = std::move(idxExpr.expression);
                    stmt.valueExpr = std::move(valExpr.expression);
                    result.lines.push_back({lineNumber, std::move(stmt)});
                    continue;
                }

                // Scalar assignment
                if (!isValidVariableName(lhsRaw)) {
                    result.errors.push_back({lineNumber, "Invalid variable name '" + lhsRaw + "'"});
                    continue;
                }
                if (isReadonlySessionSystemVariable(lhsRaw)) {
                    result.errors.push_back({lineNumber, "Read-only system variable '" + lhsRaw + "'"});
                    continue;
                }

                BasicExpressionParseResult expression = BasicExpressionParser::parse(rhsRaw);
                if (!expression.success || !expression.expression) {
                    result.errors.push_back({lineNumber, "LET expression error: " + expression.error});
                    continue;
                }

                BasicAst::LetStmt stmt{};
                stmt.variableName = lhsRaw;
                stmt.expression = std::move(expression.expression);
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "DIM") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                // Expect: varName(sizeExpr)
                const std::size_t lparenPos = rest.find('(');
                if (lparenPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "DIM requires '(' after variable name"});
                    continue;
                }
                const std::string baseName = trim(rest.substr(0, lparenPos));
                if (!isValidVariableName(baseName)) {
                    result.errors.push_back({lineNumber, "Invalid array variable name '" + baseName + "'"});
                    continue;
                }
                if (isReadonlySessionSystemVariable(baseName)) {
                    result.errors.push_back({lineNumber, "Read-only system variable '" + baseName + "'"});
                    continue;
                }
                const std::size_t rparenPos = rest.rfind(')');
                if (rparenPos == std::string::npos || rparenPos <= lparenPos) {
                    result.errors.push_back({lineNumber, "DIM missing closing ')'"});
                    continue;
                }
                const std::string sizeRaw = trim(rest.substr(lparenPos + 1, rparenPos - lparenPos - 1));
                if (sizeRaw.empty()) {
                    result.errors.push_back({lineNumber, "DIM size expression cannot be empty"});
                    continue;
                }
                BasicExpressionParseResult sizeExpr = BasicExpressionParser::parse(sizeRaw);
                if (!sizeExpr.success || !sizeExpr.expression) {
                    result.errors.push_back({lineNumber, "DIM size error: " + sizeExpr.error});
                    continue;
                }
                BasicAst::DimStmt stmt{};
                stmt.variableName = baseName;
                stmt.sizeExpr = std::move(sizeExpr.expression);
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "INPUT") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (rest.empty()) {
                    result.errors.push_back({lineNumber, "INPUT requires a variable name"});
                    continue;
                }
                std::istringstream varIss(rest);
                std::vector<std::string> inputTokens;
                std::string inputTok;
                while (varIss >> inputTok) {
                    inputTokens.push_back(inputTok);
                }
                if (inputTokens.size() != 1) {
                    result.errors.push_back({lineNumber, "INPUT requires a variable name"});
                    continue;
                }
                const std::string varName = inputTokens[0];
                if (!isValidVariableName(varName)) {
                    result.errors.push_back({lineNumber, "Invalid variable name '" + varName + "'"});
                    continue;
                }
                if (isReadonlySessionSystemVariable(varName)) {
                    result.errors.push_back({lineNumber, "Read-only system variable '" + varName + "'"});
                    continue;
                }

                result.lines.push_back({lineNumber, BasicAst::InputStmt{varName}});
                continue;
            }

            if (op == "GOTO") {
                std::string targetRaw;
                std::getline(iss, targetRaw);
                int targetLine = 0;
                if (!parsePositiveLineNumber(targetRaw, targetLine)) {
                    result.errors.push_back({lineNumber, "GOTO requires a line number"});
                    continue;
                }
                result.lines.push_back({lineNumber, BasicAst::GotoStmt{targetLine}});
                continue;
            }

            if (op == "GOSUB") {
                std::string targetRaw;
                std::getline(iss, targetRaw);
                int targetLine = 0;
                if (!parsePositiveLineNumber(targetRaw, targetLine)) {
                    result.errors.push_back({lineNumber, "GOSUB requires a line number"});
                    continue;
                }
                result.lines.push_back({lineNumber, BasicAst::GosubStmt{targetLine}});
                continue;
            }

            if (op == "RETURN") {
                std::string rest;
                std::getline(iss, rest);
                if (!trim(rest).empty()) {
                    result.errors.push_back({lineNumber, "RETURN takes no arguments"});
                    continue;
                }
                result.lines.push_back({lineNumber, BasicAst::ReturnStmt{}});
                continue;
            }

            if (op == "CLEAR") {
                std::string rest;
                std::getline(iss, rest);
                if (!trim(rest).empty()) {
                    result.errors.push_back({lineNumber, "CLEAR takes no arguments"});
                    continue;
                }
                result.lines.push_back({lineNumber, BasicAst::ClearStmt{}});
                continue;
            }

            if (op == "OPEN") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (rest.empty()) {
                    result.errors.push_back({lineNumber, "OPEN requires a file expression and TO <filevar>"});
                    continue;
                }

                std::optional<BasicAst::BranchArm> elseArm;
                std::string mainPart = rest;
                const std::string restUpper = uppercase(rest);
                const std::size_t elsePos = findKeywordToken(restUpper, "ELSE");
                if (elsePos != std::string::npos) {
                    mainPart = trim(rest.substr(0, elsePos));
                    const std::string elseRaw = trim(rest.substr(elsePos + 4));
                    BasicAst::BranchArm parsedElseArm{};
                    std::string armError;
                    if (!parseBranchArmSpec(elseRaw, "OPEN ELSE", parsedElseArm, armError)) {
                        result.errors.push_back({lineNumber, armError});
                        continue;
                    }
                    elseArm = std::move(parsedElseArm);
                }

                const std::string mainUpper = uppercase(mainPart);
                const std::size_t toPos = findKeywordToken(mainUpper, "TO");
                if (toPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "OPEN requires TO <filevar>"});
                    continue;
                }

                const std::string fileExprRaw = trim(mainPart.substr(0, toPos));
                const std::string fileVar = trim(mainPart.substr(toPos + 2));
                if (fileExprRaw.empty()) {
                    result.errors.push_back({lineNumber, "OPEN requires a file expression"});
                    continue;
                }
                if (!isValidVariableName(fileVar)) {
                    result.errors.push_back({lineNumber, "OPEN requires a valid file variable name"});
                    continue;
                }

                BasicExpressionParseResult fileExpr = BasicExpressionParser::parse(fileExprRaw);
                if (!fileExpr.success || !fileExpr.expression) {
                    result.errors.push_back({lineNumber, "OPEN file expression error: " + fileExpr.error});
                    continue;
                }

                BasicAst::OpenStmt stmt{};
                stmt.fileExpr = std::move(fileExpr.expression);
                stmt.fileVar = fileVar;
                stmt.elseArm = elseArm;
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "READ") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (rest.empty()) {
                    result.errors.push_back({lineNumber, "READ requires REC FROM FVAR, ID"});
                    continue;
                }

                std::optional<BasicAst::BranchArm> elseArm;
                std::string mainPart = rest;
                const std::string restUpper = uppercase(rest);
                const std::size_t elsePos = findKeywordToken(restUpper, "ELSE");
                if (elsePos != std::string::npos) {
                    mainPart = trim(rest.substr(0, elsePos));
                    const std::string elseRaw = trim(rest.substr(elsePos + 4));
                    BasicAst::BranchArm parsedElseArm{};
                    std::string armError;
                    if (!parseBranchArmSpec(elseRaw, "READ ELSE", parsedElseArm, armError)) {
                        result.errors.push_back({lineNumber, armError});
                        continue;
                    }
                    elseArm = std::move(parsedElseArm);
                }

                const std::string mainUpper = uppercase(mainPart);
                const std::size_t fromPos = findKeywordToken(mainUpper, "FROM");
                if (fromPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "READ requires FROM"});
                    continue;
                }
                const std::string targetVar = trim(mainPart.substr(0, fromPos));
                if (!isValidVariableName(targetVar)) {
                    result.errors.push_back({lineNumber, "READ requires a valid target variable name"});
                    continue;
                }

                const std::string afterFrom = trim(mainPart.substr(fromPos + 4));
                const std::size_t commaPos = afterFrom.find(',');
                if (commaPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "READ requires FVAR, ID"});
                    continue;
                }
                const std::string fileVar = trim(afterFrom.substr(0, commaPos));
                const std::string idExprRaw = trim(afterFrom.substr(commaPos + 1));
                if (!isValidVariableName(fileVar)) {
                    result.errors.push_back({lineNumber, "READ requires a valid file variable name"});
                    continue;
                }
                if (idExprRaw.empty()) {
                    result.errors.push_back({lineNumber, "READ requires an ID expression"});
                    continue;
                }

                BasicExpressionParseResult idExpr = BasicExpressionParser::parse(idExprRaw);
                if (!idExpr.success || !idExpr.expression) {
                    result.errors.push_back({lineNumber, "READ ID expression error: " + idExpr.error});
                    continue;
                }

                BasicAst::ReadStmt stmt{};
                stmt.targetVar = targetVar;
                stmt.fileVar = fileVar;
                stmt.idExpr = std::move(idExpr.expression);
                stmt.elseArm = elseArm;
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "WRITE") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (rest.empty()) {
                    result.errors.push_back({lineNumber, "WRITE requires REC ON FVAR, ID"});
                    continue;
                }

                std::optional<BasicAst::BranchArm> elseArm;
                std::string mainPart = rest;
                const std::string restUpper = uppercase(rest);
                const std::size_t elsePos = findKeywordToken(restUpper, "ELSE");
                if (elsePos != std::string::npos) {
                    mainPart = trim(rest.substr(0, elsePos));
                    const std::string elseRaw = trim(rest.substr(elsePos + 4));
                    BasicAst::BranchArm parsedElseArm{};
                    std::string armError;
                    if (!parseBranchArmSpec(elseRaw, "WRITE ELSE", parsedElseArm, armError)) {
                        result.errors.push_back({lineNumber, armError});
                        continue;
                    }
                    elseArm = std::move(parsedElseArm);
                }

                const std::string mainUpper = uppercase(mainPart);
                const std::size_t onPos = findKeywordToken(mainUpper, "ON");
                if (onPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "WRITE requires ON"});
                    continue;
                }
                const std::string valueExprRaw = trim(mainPart.substr(0, onPos));
                if (valueExprRaw.empty()) {
                    result.errors.push_back({lineNumber, "WRITE requires a value expression"});
                    continue;
                }

                const std::string afterOn = trim(mainPart.substr(onPos + 2));
                const std::size_t commaPos = afterOn.find(',');
                if (commaPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "WRITE requires FVAR, ID"});
                    continue;
                }
                const std::string fileVar = trim(afterOn.substr(0, commaPos));
                const std::string idExprRaw = trim(afterOn.substr(commaPos + 1));
                if (!isValidVariableName(fileVar)) {
                    result.errors.push_back({lineNumber, "WRITE requires a valid file variable name"});
                    continue;
                }
                if (idExprRaw.empty()) {
                    result.errors.push_back({lineNumber, "WRITE requires an ID expression"});
                    continue;
                }

                BasicExpressionParseResult valueExpr = BasicExpressionParser::parse(valueExprRaw);
                if (!valueExpr.success || !valueExpr.expression) {
                    result.errors.push_back({lineNumber, "WRITE value expression error: " + valueExpr.error});
                    continue;
                }
                BasicExpressionParseResult idExpr = BasicExpressionParser::parse(idExprRaw);
                if (!idExpr.success || !idExpr.expression) {
                    result.errors.push_back({lineNumber, "WRITE ID expression error: " + idExpr.error});
                    continue;
                }

                BasicAst::WriteStmt stmt{};
                stmt.valueExpr = std::move(valueExpr.expression);
                stmt.fileVar = fileVar;
                stmt.idExpr = std::move(idExpr.expression);
                stmt.elseArm = elseArm;
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "READNEXT") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (rest.empty()) {
                    result.errors.push_back({lineNumber, "READNEXT requires <var> FROM <filevar>"});
                    continue;
                }

                std::optional<BasicAst::BranchArm> elseArm;
                std::string mainPart = rest;
                const std::string restUpper = uppercase(rest);
                const std::size_t elsePos = findKeywordToken(restUpper, "ELSE");
                if (elsePos != std::string::npos) {
                    mainPart = trim(rest.substr(0, elsePos));
                    const std::string elseRaw = trim(rest.substr(elsePos + 4));
                    BasicAst::BranchArm parsedElseArm{};
                    std::string armError;
                    if (!parseBranchArmSpec(elseRaw, "READNEXT ELSE", parsedElseArm, armError)) {
                        result.errors.push_back({lineNumber, armError});
                        continue;
                    }
                    elseArm = std::move(parsedElseArm);
                }

                const std::string mainUpper = uppercase(mainPart);
                const std::size_t fromPos = findKeywordToken(mainUpper, "FROM");
                if (fromPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "READNEXT requires FROM"});
                    continue;
                }

                const std::string targetVar = trim(mainPart.substr(0, fromPos));
                const std::string fileVar = trim(mainPart.substr(fromPos + 4));
                if (!isValidVariableName(targetVar)) {
                    result.errors.push_back({lineNumber, "READNEXT requires a valid target variable name"});
                    continue;
                }
                if (!isValidVariableName(fileVar)) {
                    result.errors.push_back({lineNumber, "READNEXT requires a valid file variable name"});
                    continue;
                }

                BasicAst::ReadNextStmt stmt{};
                stmt.targetVar = targetVar;
                stmt.fileVar = fileVar;
                stmt.elseArm = elseArm;
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "READV") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (rest.empty()) {
                    result.errors.push_back({lineNumber, "READV requires <var> FROM <filevar>, <id>, <attr>[, <value-index>]"});
                    continue;
                }

                std::optional<BasicAst::BranchArm> elseArm;
                std::string mainPart = rest;
                const std::string restUpper = uppercase(rest);
                const std::size_t elsePos = findKeywordToken(restUpper, "ELSE");
                if (elsePos != std::string::npos) {
                    mainPart = trim(rest.substr(0, elsePos));
                    const std::string elseRaw = trim(rest.substr(elsePos + 4));
                    BasicAst::BranchArm parsedElseArm{};
                    std::string armError;
                    if (!parseBranchArmSpec(elseRaw, "READV ELSE", parsedElseArm, armError)) {
                        result.errors.push_back({lineNumber, armError});
                        continue;
                    }
                    elseArm = std::move(parsedElseArm);
                }

                const std::string mainUpper = uppercase(mainPart);
                const std::size_t fromPos = findKeywordToken(mainUpper, "FROM");
                if (fromPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "READV requires FROM"});
                    continue;
                }
                const std::string targetVar = trim(mainPart.substr(0, fromPos));
                if (!isValidVariableName(targetVar)) {
                    result.errors.push_back({lineNumber, "READV requires a valid target variable name"});
                    continue;
                }

                const std::string afterFrom = trim(mainPart.substr(fromPos + 4));
                std::stringstream argStream(afterFrom);
                std::vector<std::string> args;
                std::string arg;
                while (std::getline(argStream, arg, ',')) {
                    args.push_back(trim(arg));
                }
                if (args.size() != 3 && args.size() != 4) {
                    result.errors.push_back({lineNumber, "READV requires <filevar>, <id>, <attr>[, <value-index>]"});
                    continue;
                }
                if (!isValidVariableName(args[0])) {
                    result.errors.push_back({lineNumber, "READV requires a valid file variable name"});
                    continue;
                }
                if (args[1].empty() || args[2].empty()) {
                    result.errors.push_back({lineNumber, "READV requires non-empty ID and attribute expressions"});
                    continue;
                }

                BasicExpressionParseResult idExpr = BasicExpressionParser::parse(args[1]);
                if (!idExpr.success || !idExpr.expression) {
                    result.errors.push_back({lineNumber, "READV ID expression error: " + idExpr.error});
                    continue;
                }
                BasicExpressionParseResult attrExpr = BasicExpressionParser::parse(args[2]);
                if (!attrExpr.success || !attrExpr.expression) {
                    result.errors.push_back({lineNumber, "READV attribute expression error: " + attrExpr.error});
                    continue;
                }

                std::unique_ptr<BasicAst::Expr> valueIndexExpr;
                if (args.size() == 4) {
                    if (args[3].empty()) {
                        result.errors.push_back({lineNumber, "READV value-index expression cannot be empty"});
                        continue;
                    }
                    BasicExpressionParseResult valueIdx = BasicExpressionParser::parse(args[3]);
                    if (!valueIdx.success || !valueIdx.expression) {
                        result.errors.push_back({lineNumber, "READV value-index expression error: " + valueIdx.error});
                        continue;
                    }
                    valueIndexExpr = std::move(valueIdx.expression);
                }

                BasicAst::ReadVStmt stmt{};
                stmt.targetVar = targetVar;
                stmt.fileVar = args[0];
                stmt.idExpr = std::move(idExpr.expression);
                stmt.attrExpr = std::move(attrExpr.expression);
                stmt.valueIndexExpr = std::move(valueIndexExpr);
                stmt.elseArm = elseArm;
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "WRITEV") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (rest.empty()) {
                    result.errors.push_back({lineNumber, "WRITEV requires <value> ON <filevar>, <id>, <attr>[, <value-index>]"});
                    continue;
                }

                std::optional<BasicAst::BranchArm> elseArm;
                std::string mainPart = rest;
                const std::string restUpper = uppercase(rest);
                const std::size_t elsePos = findKeywordToken(restUpper, "ELSE");
                if (elsePos != std::string::npos) {
                    mainPart = trim(rest.substr(0, elsePos));
                    const std::string elseRaw = trim(rest.substr(elsePos + 4));
                    BasicAst::BranchArm parsedElseArm{};
                    std::string armError;
                    if (!parseBranchArmSpec(elseRaw, "WRITEV ELSE", parsedElseArm, armError)) {
                        result.errors.push_back({lineNumber, armError});
                        continue;
                    }
                    elseArm = std::move(parsedElseArm);
                }

                const std::string mainUpper = uppercase(mainPart);
                const std::size_t onPos = findKeywordToken(mainUpper, "ON");
                if (onPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "WRITEV requires ON"});
                    continue;
                }
                const std::string valueExprRaw = trim(mainPart.substr(0, onPos));
                if (valueExprRaw.empty()) {
                    result.errors.push_back({lineNumber, "WRITEV requires a value expression"});
                    continue;
                }
                BasicExpressionParseResult valueExpr = BasicExpressionParser::parse(valueExprRaw);
                if (!valueExpr.success || !valueExpr.expression) {
                    result.errors.push_back({lineNumber, "WRITEV value expression error: " + valueExpr.error});
                    continue;
                }

                const std::string afterOn = trim(mainPart.substr(onPos + 2));
                std::stringstream argStream(afterOn);
                std::vector<std::string> args;
                std::string arg;
                while (std::getline(argStream, arg, ',')) {
                    args.push_back(trim(arg));
                }
                if (args.size() != 3 && args.size() != 4) {
                    result.errors.push_back({lineNumber, "WRITEV requires <filevar>, <id>, <attr>[, <value-index>]"});
                    continue;
                }
                if (!isValidVariableName(args[0])) {
                    result.errors.push_back({lineNumber, "WRITEV requires a valid file variable name"});
                    continue;
                }
                if (args[1].empty() || args[2].empty()) {
                    result.errors.push_back({lineNumber, "WRITEV requires non-empty ID and attribute expressions"});
                    continue;
                }

                BasicExpressionParseResult idExpr = BasicExpressionParser::parse(args[1]);
                if (!idExpr.success || !idExpr.expression) {
                    result.errors.push_back({lineNumber, "WRITEV ID expression error: " + idExpr.error});
                    continue;
                }
                BasicExpressionParseResult attrExpr = BasicExpressionParser::parse(args[2]);
                if (!attrExpr.success || !attrExpr.expression) {
                    result.errors.push_back({lineNumber, "WRITEV attribute expression error: " + attrExpr.error});
                    continue;
                }

                std::unique_ptr<BasicAst::Expr> valueIndexExpr;
                if (args.size() == 4) {
                    if (args[3].empty()) {
                        result.errors.push_back({lineNumber, "WRITEV value-index expression cannot be empty"});
                        continue;
                    }
                    BasicExpressionParseResult valueIdx = BasicExpressionParser::parse(args[3]);
                    if (!valueIdx.success || !valueIdx.expression) {
                        result.errors.push_back({lineNumber, "WRITEV value-index expression error: " + valueIdx.error});
                        continue;
                    }
                    valueIndexExpr = std::move(valueIdx.expression);
                }

                BasicAst::WriteVStmt stmt{};
                stmt.valueExpr = std::move(valueExpr.expression);
                stmt.fileVar = args[0];
                stmt.idExpr = std::move(idExpr.expression);
                stmt.attrExpr = std::move(attrExpr.expression);
                stmt.valueIndexExpr = std::move(valueIndexExpr);
                stmt.elseArm = elseArm;
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "CLOSE") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (!isValidVariableName(rest)) {
                    result.errors.push_back({lineNumber, "CLOSE requires a valid file variable name"});
                    continue;
                }
                result.lines.push_back({lineNumber, BasicAst::CloseStmt{rest}});
                continue;
            }

            if (op == "FOR") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);

                // Find '='
                const std::size_t eqPos = rest.find('=');
                if (eqPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "FOR requires '='"});
                    continue;
                }
                const std::string varRaw = trim(rest.substr(0, eqPos));
                if (!isValidVariableName(varRaw)) {
                    result.errors.push_back({lineNumber, "FOR requires a valid variable name"});
                    continue;
                }
                if (isReadonlySessionSystemVariable(varRaw)) {
                    result.errors.push_back({lineNumber, "Read-only system variable '" + varRaw + "'"});
                    continue;
                }

                // Split remainder on TO keyword
                const std::string afterEq = trim(rest.substr(eqPos + 1));
                const std::string afterEqUpper = uppercase(afterEq);
                const std::size_t toPos = findKeywordToken(afterEqUpper, "TO");
                if (toPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "FOR requires TO"});
                    continue;
                }
                const std::string initRaw = trim(afterEq.substr(0, toPos));
                if (initRaw.empty()) {
                    result.errors.push_back({lineNumber, "FOR requires an initial value"});
                    continue;
                }

                // Split the part after TO on optional STEP keyword
                const std::string afterTo = trim(afterEq.substr(toPos + 2));
                const std::string afterToUpper = uppercase(afterTo);
                const std::size_t stepPos = findKeywordToken(afterToUpper, "STEP");

                std::string limitRaw;
                std::string stepRaw;
                if (stepPos != std::string::npos) {
                    limitRaw = trim(afterTo.substr(0, stepPos));
                    stepRaw = trim(afterTo.substr(stepPos + 4));
                } else {
                    limitRaw = afterTo;
                }

                if (limitRaw.empty()) {
                    result.errors.push_back({lineNumber, "FOR requires a limit value"});
                    continue;
                }

                BasicExpressionParseResult initExpr = BasicExpressionParser::parse(initRaw);
                if (!initExpr.success || !initExpr.expression) {
                    result.errors.push_back({lineNumber, "FOR init error: " + initExpr.error});
                    continue;
                }
                BasicExpressionParseResult limitExpr = BasicExpressionParser::parse(limitRaw);
                if (!limitExpr.success || !limitExpr.expression) {
                    result.errors.push_back({lineNumber, "FOR limit error: " + limitExpr.error});
                    continue;
                }

                BasicAst::ForStmt stmt{};
                stmt.variableName = varRaw;
                stmt.initExpr = std::move(initExpr.expression);
                stmt.limitExpr = std::move(limitExpr.expression);
                if (!stepRaw.empty()) {
                    BasicExpressionParseResult stepExpr = BasicExpressionParser::parse(stepRaw);
                    if (!stepExpr.success || !stepExpr.expression) {
                        result.errors.push_back({lineNumber, "FOR STEP error: " + stepExpr.error});
                        continue;
                    }
                    stmt.stepExpr = std::move(stepExpr.expression);
                }
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "NEXT") {
                std::string rest;
                std::getline(iss, rest);
                const std::string varName = trim(rest);
                // Variable name is optional in Pick BASIC; omitting it matches the innermost frame.
                if (!varName.empty() && !isValidVariableName(varName)) {
                    result.errors.push_back({lineNumber, "NEXT requires a valid variable name"});
                    continue;
                }
                if (!varName.empty() && isReadonlySessionSystemVariable(varName)) {
                    result.errors.push_back({lineNumber, "Read-only system variable '" + varName + "'"});
                    continue;
                }
                result.lines.push_back({lineNumber, BasicAst::NextStmt{varName}});
                continue;
            }

            if (op == "IF") {
                std::string rest;
                std::getline(iss, rest);
                rest = trim(rest);
                if (rest.empty()) {
                    result.errors.push_back({lineNumber, "IF requires THEN <line>"});
                    continue;
                }
                const std::string restUpper = uppercase(rest);
                const std::size_t thenPos = findKeywordToken(restUpper, "THEN");
                if (thenPos == std::string::npos) {
                    result.errors.push_back({lineNumber, "IF requires THEN <line>"});
                    continue;
                }

                const std::string conditionExpr = trim(rest.substr(0, thenPos));
                if (conditionExpr.empty()) {
                    result.errors.push_back({lineNumber, "IF requires a condition expression"});
                    continue;
                }

                std::string branchPart = trim(rest.substr(thenPos + 4));
                if (branchPart.empty()) {
                    result.errors.push_back({lineNumber, "IF THEN requires a line number or statement"});
                    continue;
                }
                const std::string branchUpper = uppercase(branchPart);
                const std::size_t elsePos = findKeywordToken(branchUpper, "ELSE");

                std::string thenRaw = branchPart;
                std::optional<std::string> elseRaw;
                if (elsePos != std::string::npos) {
                    thenRaw = trim(branchPart.substr(0, elsePos));
                    elseRaw = trim(branchPart.substr(elsePos + 4));
                } else {
                    thenRaw = trim(thenRaw);
                }

                BasicAst::BranchArm thenArm{};
                std::string armError;
                if (!parseBranchArmSpec(thenRaw, "IF THEN", thenArm, armError)) {
                    result.errors.push_back({lineNumber, armError});
                    continue;
                }

                std::optional<BasicAst::BranchArm> elseArm;
                if (elseRaw) {
                    BasicAst::BranchArm parsedElseArm{};
                    if (!parseBranchArmSpec(*elseRaw, "IF ELSE", parsedElseArm, armError)) {
                        result.errors.push_back({lineNumber, armError});
                        continue;
                    }
                    elseArm = std::move(parsedElseArm);
                }

                BasicExpressionParseResult condition = BasicExpressionParser::parse(conditionExpr);
                if (!condition.success || !condition.expression) {
                    result.errors.push_back({lineNumber, "IF condition error: " + condition.error});
                    continue;
                }

                BasicAst::IfStmt stmt{};
                stmt.condition = std::move(condition.expression);
                stmt.thenArm = std::move(thenArm);
                stmt.elseArm = std::move(elseArm);
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "PRINT") {
                std::string exprText;
                std::getline(iss, exprText);
                exprText = trim(exprText);
                bool suppressEol = false;
                if (!exprText.empty() && exprText.back() == ';') {
                    suppressEol = true;
                    exprText = trim(exprText.substr(0, exprText.size() - 1));
                }
                if (exprText.empty()) {
                    result.errors.push_back({lineNumber, "PRINT requires an expression"});
                    continue;
                }

                BasicExpressionParseResult expression = BasicExpressionParser::parse(exprText);
                if (!expression.success || !expression.expression) {
                    result.errors.push_back({lineNumber, "PRINT expression error: " + expression.error});
                    continue;
                }

                BasicAst::PrintStmt stmt{};
                stmt.suppressEol = suppressEol;
                stmt.expression = std::move(expression.expression);
                result.lines.push_back({lineNumber, std::move(stmt)});
                continue;
            }

            if (op == "STOP") {
                std::string rest;
                std::getline(iss, rest);
                if (!trim(rest).empty()) {
                    result.errors.push_back({lineNumber, "STOP takes no arguments"});
                    continue;
                }
                result.lines.push_back({lineNumber, BasicAst::StopStmt{}});
                continue;
            }

            if (op == "END") {
                std::string rest;
                std::getline(iss, rest);
                if (!trim(rest).empty()) {
                    result.errors.push_back({lineNumber, "END takes no arguments"});
                    continue;
                }
                result.lines.push_back({lineNumber, BasicAst::EndStmt{}});
                continue;
            }

            result.errors.push_back({lineNumber, "Unknown keyword '" + op + "'"});
        }

        result.success = result.errors.empty();
        return result;
    }
} // namespace PickShell
