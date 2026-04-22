#include "BasicProgram.h"

#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PickShell {
    namespace {
        std::string uppercase(std::string text) {
            for (char &c: text) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return text;
        }

        bool parsePositiveLineNumber(const std::string &token, int &lineNumber) {
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

        std::string joinTokens(const std::vector<std::string> &tokens) {
            std::string out;
            for (std::size_t i = 0; i < tokens.size(); ++i) {
                if (i > 0) {
                    out += ' ';
                }
                out += tokens[i];
            }
            return out;
        }

        std::string rewriteRenumberTargets(
            const std::string &text,
            const std::unordered_map<int, int> &lineMap) {
            std::istringstream iss(text);
            std::vector<std::string> tokens;
            std::string tok;
            while (iss >> tok) {
                tokens.push_back(tok);
            }
            if (tokens.empty()) {
                return text;
            }

            const std::string op = uppercase(tokens[0]);
            if (op == "GOTO" && tokens.size() == 2) {
                int target = 0;
                if (!parsePositiveLineNumber(tokens[1], target)) {
                    return text;
                }
                const auto it = lineMap.find(target);
                if (it != lineMap.end()) {
                    tokens[1] = std::to_string(it->second);
                    return joinTokens(tokens);
                }
                return text;
            }

            if (op == "IF") {
                std::size_t thenPos = std::string::npos;
                std::size_t elsePos = std::string::npos;
                for (std::size_t i = 1; i < tokens.size(); ++i) {
                    const std::string key = uppercase(tokens[i]);
                    if (key == "THEN" && thenPos == std::string::npos) {
                        thenPos = i;
                    } else if (key == "ELSE") {
                        elsePos = i;
                    }
                }

                if (thenPos == std::string::npos || thenPos + 1 >= tokens.size()) {
                    return text;
                }

                if (elsePos == std::string::npos) {
                    if (thenPos + 2 != tokens.size()) {
                        return text;
                    }
                    int thenTarget = 0;
                    if (!parsePositiveLineNumber(tokens[thenPos + 1], thenTarget)) {
                        return text;
                    }
                    const auto thenIt = lineMap.find(thenTarget);
                    if (thenIt != lineMap.end()) {
                        tokens[thenPos + 1] = std::to_string(thenIt->second);
                        return joinTokens(tokens);
                    }
                    return text;
                }

                if (elsePos != thenPos + 2 || elsePos + 1 != tokens.size() - 1) {
                    return text;
                }
                int thenTarget = 0;
                int elseTarget = 0;
                if (!parsePositiveLineNumber(tokens[thenPos + 1], thenTarget) ||
                    !parsePositiveLineNumber(tokens[elsePos + 1], elseTarget)) {
                    return text;
                }
                const auto thenIt = lineMap.find(thenTarget);
                const auto elseIt = lineMap.find(elseTarget);
                if (thenIt != lineMap.end()) {
                    tokens[thenPos + 1] = std::to_string(thenIt->second);
                }
                if (elseIt != lineMap.end()) {
                    tokens[elsePos + 1] = std::to_string(elseIt->second);
                }
                return joinTokens(tokens);
            }

            return text;
        }
    } // namespace

    void BasicProgram::setName(std::optional<std::string> name) {
        name_ = std::move(name);
    }

    void BasicProgram::clearLines() {
        lines_.clear();
    }

    void BasicProgram::setLine(const int lineNumber, std::string text) {
        lines_[lineNumber] = std::move(text);
    }

    bool BasicProgram::hasLine(const int lineNumber) const {
        return lines_.find(lineNumber) != lines_.end();
    }

    std::optional<std::string> BasicProgram::lineText(const int lineNumber) const {
        const auto it = lines_.find(lineNumber);
        if (it == lines_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool BasicProgram::deleteLine(const int lineNumber) {
        return lines_.erase(lineNumber) != 0U;
    }

    std::size_t BasicProgram::deleteRange(const int startLine, const int endLine) {
        if (startLine > endLine) {
            return 0;
        }
        const auto begin = lines_.lower_bound(startLine);
        const auto end = lines_.upper_bound(endLine);
        const std::size_t n = static_cast<std::size_t>(std::distance(begin, end));
        lines_.erase(begin, end);
        return n;
    }

    void BasicProgram::renumber(const int first, const int increment) {
        std::unordered_map<int, int> lineMap;
        int mapped = first;
        for (const auto &[oldLine, _]: lines_) {
            lineMap[oldLine] = mapped;
            mapped += increment;
        }

        std::map<int, std::string> renumbered;
        int next = first;
        for (const auto &[_, text]: lines_) {
            renumbered[next] = rewriteRenumberTargets(text, lineMap);
            next += increment;
        }
        lines_ = std::move(renumbered);
    }

    void BasicProgram::list(std::ostream &out) const {
        for (const auto &[lineNumber, text]: lines_) {
            out << lineNumber;
            if (!text.empty()) {
                out << ' ' << text;
            }
            out << '\n';
        }
    }
} // namespace PickShell
