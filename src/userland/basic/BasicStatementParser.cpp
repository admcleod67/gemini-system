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

        std::optional<std::string> parseStringLiteral(const std::string &expr) {
            if (expr.size() >= 2 && expr.front() == '"' && expr.back() == '"') {
                return expr.substr(1, expr.size() - 2);
            }
            return std::nullopt;
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

                BasicAst::PrintStmt stmt{};
                stmt.suppressEol = suppressEol;
                const std::optional<std::string> asString = parseStringLiteral(exprText);
                if (asString) {
                    stmt.stringLiteral = *asString;
                    result.lines.push_back({lineNumber, std::move(stmt)});
                    continue;
                }

                BasicExpressionParseResult expression = BasicExpressionParser::parse(exprText);
                if (!expression.success || !expression.expression) {
                    result.errors.push_back({lineNumber, "PRINT expression error: " + expression.error});
                    continue;
                }
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
