#include "ProcInterpreter.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace PickShell {
    namespace {
        std::string trim(const std::string &text) {
            std::size_t first = 0;
            while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])) != 0) {
                ++first;
            }
            std::size_t last = text.size();
            while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])) != 0) {
                --last;
            }
            return text.substr(first, last - first);
        }

        std::string upperAscii(std::string text) {
            std::transform(text.begin(), text.end(), text.begin(), [](const unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            return text;
        }

        bool isReadonlySessionAtTarget(const std::string &token) {
            const std::string c = upperAscii(token);
            return c == "@USERNO" || c == "@ACCOUNT" || c == "@LOGNAME" || c == "@DEFDATA";
        }

        std::vector<std::string> tokenize(const std::string &line) {
            std::vector<std::string> tokens;
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            return tokens;
        }

        std::string joinTokens(const std::vector<std::string> &tokens, const std::size_t startIndex) {
            std::string out;
            for (std::size_t i = startIndex; i < tokens.size(); ++i) {
                if (i > startIndex) {
                    out += ' ';
                }
                out += tokens[i];
            }
            return out;
        }

        std::string substituteToken(const std::string &token,
                                    const std::unordered_map<std::string, std::string> &variables,
                                    const ProcInterpreter::SessionAtFn *sessionAt) {
            const auto it = variables.find(token);
            if (it != variables.end()) {
                return it->second;
            }
            if (sessionAt != nullptr && static_cast<bool>(*sessionAt)) {
                if (const std::optional<std::string> v = (*sessionAt)(token)) {
                    return *v;
                }
            }
            return token;
        }

        std::vector<std::string> substituteTokens(const std::vector<std::string> &tokens,
                                                    const std::size_t startIndex,
                                                    const std::unordered_map<std::string, std::string> &variables,
                                                    const ProcInterpreter::SessionAtFn *sessionAt) {
            std::vector<std::string> out;
            out.reserve(tokens.size() - std::min(startIndex, tokens.size()));
            for (std::size_t i = startIndex; i < tokens.size(); ++i) {
                out.push_back(substituteToken(tokens[i], variables, sessionAt));
            }
            return out;
        }
    } // namespace

    bool ProcInterpreter::runScript(const std::vector<std::string> &lines,
                                    const std::vector<std::string> &params,
                                    std::istream *inputStream,
                                    std::ostream &out,
                                    const TclBridgeFn &tclBridgeFn,
                                    const SessionAtFn &sessionAt) const {
        const SessionAtFn *const sessionPtr = static_cast<bool>(sessionAt) ? &sessionAt : nullptr;

        std::unordered_map<std::string, int> labels;
        labels.reserve(lines.size());
        for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
            const std::string line = trim(lines[static_cast<std::size_t>(i)]);
            if (line.empty() || line.back() != ':') {
                continue;
            }
            const std::string label = trim(line.substr(0, line.size() - 1));
            if (label.empty()) {
                out << "Error: Invalid label line\n";
                return false;
            }
            const std::string canon = upperAscii(label);
            if (labels.find(canon) != labels.end()) {
                out << "Error: Duplicate label: " << canon << "\n";
                return false;
            }
            labels[canon] = i;
        }

        std::unordered_map<std::string, std::string> variables;
        for (std::size_t i = 0; i < params.size(); ++i) {
            variables["P" + std::to_string(i + 1)] = params[i];
        }

        std::istream &in = inputStream ? *inputStream : std::cin;
        int pc = 0;
        while (pc >= 0 && pc < static_cast<int>(lines.size())) {
            const std::string rawLine = trim(lines[static_cast<std::size_t>(pc)]);
            if (rawLine.empty() || rawLine.back() == ':') {
                ++pc;
                continue;
            }

            const std::vector<std::string> tokens = tokenize(rawLine);
            if (tokens.empty()) {
                ++pc;
                continue;
            }

            const std::string keyword = upperAscii(tokens[0]);
            if (keyword == "DISPLAY") {
                out << joinTokens(substituteTokens(tokens, 1, variables, sessionPtr), 0) << "\n";
                ++pc;
                continue;
            }
            if (keyword == "INPUT") {
                if (tokens.size() != 2) {
                    out << "Error: INPUT requires a variable name\n";
                    return false;
                }
                if (isReadonlySessionAtTarget(tokens[1])) {
                    out << "Error: Read-only system variable\n";
                    return false;
                }
                std::string value;
                if (!std::getline(in, value)) {
                    value.clear();
                }
                variables[tokens[1]] = value;
                ++pc;
                continue;
            }
            if (keyword == "IF") {
                if (tokens.size() != 7 || tokens[2] != "=" || upperAscii(tokens[4]) != "THEN" || upperAscii(tokens[5]) != "GO") {
                    out << "Error: IF requires IF <lhs> = <rhs> THEN GO <label>\n";
                    return false;
                }
                const std::string lhsVal = substituteToken(tokens[1], variables, sessionPtr);
                const std::string rhsVal = substituteToken(tokens[3], variables, sessionPtr);
                if (lhsVal == rhsVal) {
                    const std::string label = upperAscii(tokens[6]);
                    const auto labelIt = labels.find(label);
                    if (labelIt == labels.end()) {
                        out << "Error: Unknown label: " << tokens[6] << "\n";
                        return false;
                    }
                    pc = labelIt->second;
                } else {
                    ++pc;
                }
                continue;
            }
            if (keyword == "GO") {
                if (tokens.size() != 2) {
                    out << "Error: GO requires a label\n";
                    return false;
                }
                const std::string label = upperAscii(tokens[1]);
                const auto labelIt = labels.find(label);
                if (labelIt == labels.end()) {
                    out << "Error: Unknown label: " << tokens[1] << "\n";
                    return false;
                }
                pc = labelIt->second;
                continue;
            }
            if (keyword == "TCL") {
                const std::string cmd = joinTokens(substituteTokens(tokens, 1, variables, sessionPtr), 0);
                tclBridgeFn(cmd, out);
                ++pc;
                continue;
            }
            if (keyword == "END") {
                return true;
            }

            if (tokens.size() >= 3 && tokens[1] == "=") {
                if (isReadonlySessionAtTarget(tokens[0])) {
                    out << "Error: Read-only system variable\n";
                    return false;
                }
                variables[tokens[0]] = joinTokens(substituteTokens(tokens, 2, variables, sessionPtr), 0);
                ++pc;
                continue;
            }

            out << "Error: Unknown PROC statement\n";
            return false;
        }
        return true;
    }
} // namespace PickShell
