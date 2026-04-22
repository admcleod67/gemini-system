#include "BasicCompiler.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

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

        std::string uppercase(std::string s) {
            for (char &c: s) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return s;
        }

        bool isValidVariableName(const std::string &token) {
            if (token.empty()) {
                return false;
            }
            const char first = token.front();
            if (std::isalpha(static_cast<unsigned char>(first)) == 0) {
                return false;
            }
            for (const char c: token) {
                if (std::isalnum(static_cast<unsigned char>(c)) == 0) {
                    return false;
                }
            }
            return true;
        }

        bool parseIntLiteral(const std::string &token, int &value) {
            if (token.empty()) {
                return false;
            }
            char *end = nullptr;
            errno = 0;
            const long v = std::strtol(token.c_str(), &end, 10);
            if (errno != 0 || end == nullptr || *end != '\0') {
                return false;
            }
            if (v < static_cast<long>(std::numeric_limits<int>::min()) ||
                v > static_cast<long>(std::numeric_limits<int>::max())) {
                return false;
            }
            value = static_cast<int>(v);
            return true;
        }

        std::optional<std::string> parseStringLiteral(const std::string &expr) {
            if (expr.size() >= 2 && expr.front() == '"' && expr.back() == '"') {
                return expr.substr(1, expr.size() - 2);
            }
            return std::nullopt;
        }

        PickVM::Instruction makeNoOperandInstruction(const PickVM::OpCode op) {
            return PickVM::Instruction{op, PickVM::Value{}};
        }
    } // namespace

    BasicCompileResult BasicCompiler::compile(const BasicProgram &program) const {
        BasicCompileResult result;
        bool explicitEnd = false;

        for (const auto &[lineNumber, sourceText]: program.lines()) {
            const std::string line = trim(sourceText);
            if (line.empty()) {
                continue;
            }

            std::istringstream iss(line);
            std::string keyword;
            iss >> keyword;
            const std::string op = uppercase(keyword);

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

                int intValue = 0;
                if (parseIntLiteral(rhsRaw, intValue)) {
                    result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, intValue});
                } else if (isValidVariableName(rhsRaw)) {
                    result.program.push_back(PickVM::Instruction{PickVM::OpCode::LoadVar, uppercase(rhsRaw)});
                } else {
                    result.errors.push_back({lineNumber, "LET expression must be an int literal or variable"});
                    continue;
                }

                result.program.push_back(PickVM::Instruction{PickVM::OpCode::StoreVar, uppercase(lhsRaw)});
                continue;
            }

            if (op == "PRINT") {
                std::string expr;
                std::getline(iss, expr);
                expr = trim(expr);
                if (expr.empty()) {
                    result.errors.push_back({lineNumber, "PRINT requires an expression"});
                    continue;
                }

                const std::optional<std::string> asString = parseStringLiteral(expr);
                if (asString) {
                    result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushStr, *asString});
                    result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintStr));
                    continue;
                }

                int intValue = 0;
                if (parseIntLiteral(expr, intValue)) {
                    result.program.push_back(PickVM::Instruction{PickVM::OpCode::PushInt, intValue});
                    result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintInt));
                    continue;
                }

                if (isValidVariableName(expr)) {
                    result.program.push_back(PickVM::Instruction{PickVM::OpCode::LoadVar, uppercase(expr)});
                    result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::PrintInt));
                    continue;
                }

                result.errors.push_back({lineNumber, "PRINT expression must be int variable, int literal, or string literal"});
                continue;
            }

            if (op == "END") {
                std::string rest;
                std::getline(iss, rest);
                if (!trim(rest).empty()) {
                    result.errors.push_back({lineNumber, "END takes no arguments"});
                    continue;
                }
                result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
                explicitEnd = true;
                continue;
            }

            result.errors.push_back({lineNumber, "Unknown keyword '" + op + "'"});
        }

        if (!result.errors.empty()) {
            result.program.clear();
            result.instructionCount = 0;
            result.labelCount = 0;
            result.success = false;
            return result;
        }

        if (!explicitEnd || result.program.empty() || result.program.back().op != PickVM::OpCode::Halt) {
            result.program.push_back(makeNoOperandInstruction(PickVM::OpCode::Halt));
        }

        result.instructionCount = result.program.size();
        result.labelCount = 0;
        result.success = true;
        return result;
    }
} // namespace PickShell
