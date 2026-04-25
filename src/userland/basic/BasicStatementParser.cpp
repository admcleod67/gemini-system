#include "BasicStatementParser.h"

#include "BasicCompileUtils.h"
#include "BasicExpressionParser.h"

#include <optional>
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
                if (!isValidVariableName(lhsRaw)) {
                    result.errors.push_back({lineNumber, "Invalid variable name '" + lhsRaw + "'"});
                    continue;
                }
                if (rhsRaw.empty()) {
                    result.errors.push_back({lineNumber, "LET requires an expression"});
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
                    result.errors.push_back({lineNumber, "IF THEN requires a line number"});
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

                int thenLine = 0;
                if (!parsePositiveLineNumber(thenRaw, thenLine)) {
                    result.errors.push_back({lineNumber, "IF THEN requires a line number"});
                    continue;
                }

                int elseLine = 0;
                if (elseRaw && !parsePositiveLineNumber(*elseRaw, elseLine)) {
                    result.errors.push_back({lineNumber, "IF ELSE requires a line number"});
                    continue;
                }

                BasicExpressionParseResult condition = BasicExpressionParser::parse(conditionExpr);
                if (!condition.success || !condition.expression) {
                    result.errors.push_back({lineNumber, "IF condition error: " + condition.error});
                    continue;
                }

                BasicAst::IfStmt stmt{};
                stmt.condition = std::move(condition.expression);
                stmt.thenLine = thenLine;
                if (elseRaw) {
                    stmt.elseLine = elseLine;
                }
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
