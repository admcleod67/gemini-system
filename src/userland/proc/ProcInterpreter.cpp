#include "ProcInterpreter.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

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

        enum class Command {
            Display,
            Input,
            If,
            Go,
            Tcl,
            End,
            Return,
            Loop,
            Repeat,
            Exit,
            ExitIf,
            Select,
            ReadNext,
            ReadU,
            WriteU,
            Release,
            Assignment,
            Unknown
        };

        struct ParsedStatement {
            Command command{Command::Unknown};
            std::vector<std::string> tokens;
        };

        struct IfParts {
            std::vector<std::string> lhsTokens;
            std::vector<std::string> rhsTokens;
            std::vector<std::string> thenTokens;
            std::vector<std::string> elseTokens;
        };

        struct LoopFrame {
            int loopPc{-1};
            int repeatPc{-1};
        };

        bool tokenEq(const std::string &token, const std::string_view target) {
            if (token.size() != target.size()) {
                return false;
            }
            for (std::size_t i = 0; i < token.size(); ++i) {
                if (std::toupper(static_cast<unsigned char>(token[i])) !=
                    std::toupper(static_cast<unsigned char>(target[i]))) {
                    return false;
                }
            }
            return true;
        }

        constexpr const char *kProcErrVar = "PROCERR";
        constexpr const char *kProcLockedToken = "?LOCKED?";

        std::optional<std::string> defaultDataFile(const ProcInterpreter::SessionAtFn *sessionAt) {
            if (sessionAt == nullptr || !static_cast<bool>(*sessionAt)) {
                return std::nullopt;
            }
            if (const std::optional<std::string> v = (*sessionAt)("@DEFDATA")) {
                if (v->empty()) {
                    return std::nullopt;
                }
                return v;
            }
            return std::nullopt;
        }

        struct FileRecordPair {
            std::string file;
            std::string id;
        };

        std::optional<FileRecordPair> parseFileRecordPair(const std::vector<std::string> &operands,
                                                          const ProcInterpreter::SessionAtFn *sessionAt) {
            if (operands.size() == 2) {
                return FileRecordPair{operands[0], operands[1]};
            }
            if (operands.size() == 1) {
                if (const std::optional<std::string> defData = defaultDataFile(sessionAt)) {
                    return FileRecordPair{*defData, operands[0]};
                }
            }
            return std::nullopt;
        }

        std::optional<FileRecordPair> parseReadUOperands(const std::vector<std::string> &substitutedOperands,
                                                         const ProcInterpreter::SessionAtFn *sessionAt) {
            if (substitutedOperands.size() == 2) {
                return FileRecordPair{substitutedOperands[0], substitutedOperands[1]};
            }
            if (substitutedOperands.size() == 1) {
                if (const std::optional<std::string> defData = defaultDataFile(sessionAt)) {
                    return FileRecordPair{*defData, substitutedOperands[0]};
                }
            }
            return std::nullopt;
        }

        void clearProcErr(std::unordered_map<std::string, std::string> &variables) {
            variables[kProcErrVar].clear();
        }

        void setProcLocked(std::unordered_map<std::string, std::string> &variables) {
            variables[kProcErrVar] = kProcLockedToken;
        }

        std::string canonicalKeyword(const std::string &token) {
            const std::string up = upperAscii(token);
            if (up == "DISPLAY" || up == "D") {
                return "DISPLAY";
            }
            if (up == "INPUT" || up == "I") {
                return "INPUT";
            }
            if (up == "IF") {
                return "IF";
            }
            if (up == "GO" || up == "G") {
                return "GO";
            }
            if (up == "TCL" || up == "T") {
                return "TCL";
            }
            if (up == "END" || up == "E") {
                return "END";
            }
            if (up == "RETURN") {
                return "RETURN";
            }
            if (up == "LOOP") {
                return "LOOP";
            }
            if (up == "REPEAT") {
                return "REPEAT";
            }
            if (up == "EXIT") {
                return "EXIT";
            }
            if (up == "EXITIF") {
                return "EXITIF";
            }
            if (up == "SELECT" || up == "S") {
                return "SELECT";
            }
            if (up == "READNEXT" || up == "RN") {
                return "READNEXT";
            }
            if (up == "READU") {
                return "READU";
            }
            if (up == "WRITEU") {
                return "WRITEU";
            }
            if (up == "RELEASE") {
                return "RELEASE";
            }
            return up;
        }

        Command commandFromCanonical(const std::string &keyword) {
            if (keyword == "DISPLAY") {
                return Command::Display;
            }
            if (keyword == "INPUT") {
                return Command::Input;
            }
            if (keyword == "IF") {
                return Command::If;
            }
            if (keyword == "GO") {
                return Command::Go;
            }
            if (keyword == "TCL") {
                return Command::Tcl;
            }
            if (keyword == "END") {
                return Command::End;
            }
            if (keyword == "RETURN") {
                return Command::Return;
            }
            if (keyword == "LOOP") {
                return Command::Loop;
            }
            if (keyword == "REPEAT") {
                return Command::Repeat;
            }
            if (keyword == "EXIT") {
                return Command::Exit;
            }
            if (keyword == "EXITIF") {
                return Command::ExitIf;
            }
            if (keyword == "SELECT") {
                return Command::Select;
            }
            if (keyword == "READNEXT") {
                return Command::ReadNext;
            }
            if (keyword == "READU") {
                return Command::ReadU;
            }
            if (keyword == "WRITEU") {
                return Command::WriteU;
            }
            if (keyword == "RELEASE") {
                return Command::Release;
            }
            return Command::Unknown;
        }

        ParsedStatement parseStatement(const std::vector<std::string> &tokens) {
            ParsedStatement parsed;
            parsed.tokens = tokens;
            if (tokens.empty()) {
                return parsed;
            }
            // Assignment disambiguation must happen before alias resolution.
            if (tokens.size() >= 3 && tokens[1] == "=") {
                parsed.command = Command::Assignment;
                return parsed;
            }
            parsed.command = commandFromCanonical(canonicalKeyword(tokens[0]));
            return parsed;
        }

        bool resolveLabelPc(const std::string &token,
                            const std::unordered_map<std::string, int> &labels,
                            std::ostream &out,
                            int &pcOut) {
            const auto labelIt = labels.find(upperAscii(token));
            if (labelIt == labels.end()) {
                out << "Error: Unknown label: " << token << "\n";
                return false;
            }
            pcOut = labelIt->second;
            return true;
        }

        bool parseIfParts(const std::vector<std::string> &tokens, IfParts &parts) {
            if (tokens.size() < 6) {
                return false;
            }
            if (canonicalKeyword(tokens[0]) != "IF") {
                return false;
            }
            std::size_t eqPos = std::string::npos;
            std::size_t thenPos = std::string::npos;
            std::size_t elsePos = std::string::npos;
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == "=" && eqPos == std::string::npos) {
                    eqPos = i;
                    continue;
                }
                if (tokenEq(tokens[i], "THEN") && thenPos == std::string::npos) {
                    thenPos = i;
                    continue;
                }
                if (tokenEq(tokens[i], "ELSE") && elsePos == std::string::npos) {
                    elsePos = i;
                    continue;
                }
            }
            if (eqPos == std::string::npos || thenPos == std::string::npos) {
                return false;
            }
            if (eqPos <= 1 || thenPos <= eqPos + 1 || thenPos + 1 >= tokens.size()) {
                return false;
            }
            if (elsePos != std::string::npos && elsePos <= thenPos + 1) {
                return false;
            }
            parts.lhsTokens.assign(tokens.begin() + 1, tokens.begin() + static_cast<long>(eqPos));
            parts.rhsTokens.assign(tokens.begin() + static_cast<long>(eqPos + 1), tokens.begin() + static_cast<long>(thenPos));
            if (elsePos == std::string::npos) {
                parts.thenTokens.assign(tokens.begin() + static_cast<long>(thenPos + 1), tokens.end());
                parts.elseTokens.clear();
            } else {
                parts.thenTokens.assign(tokens.begin() + static_cast<long>(thenPos + 1), tokens.begin() + static_cast<long>(elsePos));
                parts.elseTokens.assign(tokens.begin() + static_cast<long>(elsePos + 1), tokens.end());
            }
            return !parts.thenTokens.empty() && !parts.lhsTokens.empty() && !parts.rhsTokens.empty();
        }

        bool parseExitIfCondition(const std::vector<std::string> &tokens,
                                  std::vector<std::string> &lhsTokens,
                                  std::vector<std::string> &rhsTokens) {
            if (tokens.size() < 3) {
                return false;
            }
            std::size_t eqPos = std::string::npos;
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == "=") {
                    eqPos = i;
                    break;
                }
            }
            if (eqPos == std::string::npos || eqPos <= 1) {
                return false;
            }
            lhsTokens.assign(tokens.begin() + 1, tokens.begin() + static_cast<long>(eqPos));
            rhsTokens.assign(tokens.begin() + static_cast<long>(eqPos + 1), tokens.end());
            return !lhsTokens.empty();
        }

        std::string evalValue(const std::vector<std::string> &tokens,
                              const std::unordered_map<std::string, std::string> &variables,
                              const ProcInterpreter::SessionAtFn *sessionAt) {
            if (tokens.empty()) {
                return "";
            }
            if (tokens.size() == 1) {
                return substituteToken(tokens[0], variables, sessionAt);
            }
            return joinTokens(substituteTokens(tokens, 0, variables, sessionAt), 0);
        }

        bool buildLoopPairs(const std::vector<std::vector<std::string>> &lineTokens,
                            const std::vector<std::string> &rawLines,
                            std::vector<int> &loopToRepeat,
                            std::vector<int> &repeatToLoop,
                            std::ostream &out) {
            loopToRepeat.assign(lineTokens.size(), -1);
            repeatToLoop.assign(lineTokens.size(), -1);
            std::vector<int> loopStack;
            for (std::size_t i = 0; i < lineTokens.size(); ++i) {
                const std::string trimmed = trim(rawLines[i]);
                if (trimmed.empty() || trimmed.back() == ':') {
                    continue;
                }
                const ParsedStatement parsed = parseStatement(lineTokens[i]);
                if (parsed.command == Command::Loop) {
                    loopStack.push_back(static_cast<int>(i));
                } else if (parsed.command == Command::Repeat) {
                    if (loopStack.empty()) {
                        out << "Error: REPEAT without LOOP\n";
                        return false;
                    }
                    const int loopPc = loopStack.back();
                    loopStack.pop_back();
                    loopToRepeat[static_cast<std::size_t>(loopPc)] = static_cast<int>(i);
                    repeatToLoop[i] = loopPc;
                }
            }
            if (!loopStack.empty()) {
                out << "Error: LOOP without REPEAT\n";
                return false;
            }
            return true;
        }
    } // namespace

    bool ProcInterpreter::runScript(const std::vector<std::string> &lines,
                                    const std::vector<std::string> &params,
                                    std::istream *inputStream,
                                    std::ostream &out,
                                    const TclBridgeFn &tclBridgeFn,
                                    const SessionAtFn &sessionAt,
                                    const SelectFn &selectFn,
                                    const ReadNextFn &readNextFn,
                                    const ReadUFn &readUFn,
                                    const WriteUFn &writeUFn,
                                    const ReleaseFn &releaseFn) const {
        const SessionAtFn *const sessionPtr = static_cast<bool>(sessionAt) ? &sessionAt : nullptr;
        const SelectFn *const selectPtr = static_cast<bool>(selectFn) ? &selectFn : nullptr;
        const ReadNextFn *const readNextPtr = static_cast<bool>(readNextFn) ? &readNextFn : nullptr;
        const ReadUFn *const readUPtr = static_cast<bool>(readUFn) ? &readUFn : nullptr;
        const WriteUFn *const writeUPtr = static_cast<bool>(writeUFn) ? &writeUFn : nullptr;
        const ReleaseFn *const releasePtr = static_cast<bool>(releaseFn) ? &releaseFn : nullptr;

        std::unordered_map<std::string, int> labels;
        labels.reserve(lines.size());
        std::vector<std::vector<std::string>> lineTokens;
        lineTokens.reserve(lines.size());
        for (const auto &line : lines) {
            lineTokens.push_back(tokenize(trim(line)));
        }
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
        std::vector<int> loopToRepeat;
        std::vector<int> repeatToLoop;
        if (!buildLoopPairs(lineTokens, lines, loopToRepeat, repeatToLoop, out)) {
            return false;
        }

        std::unordered_map<std::string, std::string> variables;
        for (std::size_t i = 0; i < params.size(); ++i) {
            variables["P" + std::to_string(i + 1)] = params[i];
        }

        std::istream &in = inputStream ? *inputStream : std::cin;
        int pc = 0;
        std::vector<LoopFrame> loopFrames;

        const auto syncLoopFramesForPc = [&loopFrames](const int currentPc) {
            loopFrames.erase(std::remove_if(loopFrames.begin(),
                                            loopFrames.end(),
                                            [currentPc](const LoopFrame &f) {
                                                return currentPc <= f.loopPc || currentPc > f.repeatPc;
                                            }),
                             loopFrames.end());
        };

        std::function<bool(const std::vector<std::string> &, int &, bool)> execStatement;
        execStatement = [&](const std::vector<std::string> &tokens, int &activePc, const bool inlineStmt) -> bool {
            const ParsedStatement parsed = parseStatement(tokens);
            const auto nextLine = [&activePc]() { ++activePc; };

            if (parsed.command == Command::Display) {
                out << joinTokens(substituteTokens(tokens, 1, variables, sessionPtr), 0) << "\n";
                if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }
            if (parsed.command == Command::Input) {
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
                if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }
            if (parsed.command == Command::If) {
                IfParts parts;
                if (!parseIfParts(tokens, parts)) {
                    out << "Error: IF requires IF <lhs> = <rhs> THEN GO <label>\n";
                    return false;
                }
                const bool truthy = evalValue(parts.lhsTokens, variables, sessionPtr) ==
                                    evalValue(parts.rhsTokens, variables, sessionPtr);
                const std::vector<std::string> &branchTokens = truthy ? parts.thenTokens : parts.elseTokens;
                if (branchTokens.empty()) {
                    if (!inlineStmt) {
                        nextLine();
                    }
                    return true;
                }
                int branchPc = activePc;
                if (!execStatement(branchTokens, branchPc, true)) {
                    return false;
                }
                if (!inlineStmt) {
                    if (branchPc == activePc) {
                        nextLine();
                    } else {
                        activePc = branchPc;
                    }
                } else {
                    activePc = branchPc;
                }
                return true;
            }
            if (parsed.command == Command::Go) {
                if (tokens.size() != 2) {
                    out << "Error: GO requires a label\n";
                    return false;
                }
                return resolveLabelPc(tokens[1], labels, out, activePc);
            }
            if (parsed.command == Command::Tcl) {
                const std::string cmd = joinTokens(substituteTokens(tokens, 1, variables, sessionPtr), 0);
                tclBridgeFn(cmd, out);
                if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }
            if (parsed.command == Command::End) {
                activePc = static_cast<int>(lines.size());
                return true;
            }
            if (parsed.command == Command::Return) {
                activePc = static_cast<int>(lines.size());
                return true;
            }
            if (parsed.command == Command::Loop) {
                const int repeatPc = loopToRepeat[static_cast<std::size_t>(activePc)];
                if (repeatPc < 0) {
                    out << "Error: LOOP without REPEAT\n";
                    return false;
                }
                if (loopFrames.empty() || loopFrames.back().loopPc != activePc) {
                    loopFrames.push_back(LoopFrame{activePc, repeatPc});
                }
                if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }
            if (parsed.command == Command::Repeat) {
                const int loopPc = repeatToLoop[static_cast<std::size_t>(activePc)];
                if (loopPc < 0) {
                    out << "Error: REPEAT without LOOP\n";
                    return false;
                }
                activePc = loopPc + 1;
                return true;
            }
            if (parsed.command == Command::Exit) {
                syncLoopFramesForPc(activePc);
                if (loopFrames.empty()) {
                    out << "Error: EXIT outside LOOP\n";
                    return false;
                }
                const LoopFrame frame = loopFrames.back();
                loopFrames.pop_back();
                activePc = frame.repeatPc + 1;
                return true;
            }
            if (parsed.command == Command::ExitIf) {
                std::vector<std::string> lhsTokens;
                std::vector<std::string> rhsTokens;
                if (!parseExitIfCondition(tokens, lhsTokens, rhsTokens)) {
                    out << "Error: EXITIF requires EXITIF <lhs> = <rhs>\n";
                    return false;
                }
                if (evalValue(lhsTokens, variables, sessionPtr) == evalValue(rhsTokens, variables, sessionPtr)) {
                    syncLoopFramesForPc(activePc);
                    if (loopFrames.empty()) {
                        out << "Error: EXIT outside LOOP\n";
                        return false;
                    }
                    const LoopFrame frame = loopFrames.back();
                    loopFrames.pop_back();
                    activePc = frame.repeatPc + 1;
                } else if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }
            if (parsed.command == Command::Select) {
                if (tokens.size() != 2) {
                    out << "Error: SELECT requires a filename\n";
                    return false;
                }
                if (selectPtr == nullptr) {
                    out << "Error: SELECT not available in this PROC host\n";
                    return false;
                }
                std::string error;
                if (!(*selectPtr)(substituteToken(tokens[1], variables, sessionPtr), error)) {
                    out << "Error: " << error << "\n";
                    return false;
                }
                if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }
            if (parsed.command == Command::ReadNext) {
                if (tokens.size() != 2) {
                    out << "Error: READNEXT requires a variable name\n";
                    return false;
                }
                if (isReadonlySessionAtTarget(tokens[1])) {
                    out << "Error: Read-only system variable\n";
                    return false;
                }
                if (readNextPtr == nullptr) {
                    out << "Error: READNEXT not available in this PROC host\n";
                    return false;
                }
                std::string error;
                if (const std::optional<std::string> nextId = (*readNextPtr)(error)) {
                    variables[tokens[1]] = *nextId;
                } else {
                    if (!error.empty()) {
                        out << "Error: " << error << "\n";
                        return false;
                    }
                    variables[tokens[1]].clear();
                }
                if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }
            if (parsed.command == Command::ReadU) {
                if (tokens.size() < 3) {
                    out << "Error: READU requires a variable and file/record operands\n";
                    return false;
                }
                if (isReadonlySessionAtTarget(tokens[1])) {
                    out << "Error: Read-only system variable\n";
                    return false;
                }
                if (readUPtr == nullptr) {
                    out << "Error: READU not available in this PROC host\n";
                    return false;
                }
                const std::vector<std::string> operands =
                    substituteTokens(tokens, 2, variables, sessionPtr);
                const std::optional<FileRecordPair> target = parseReadUOperands(operands, sessionPtr);
                if (!target.has_value()) {
                    out << "Error: READU requires <var> <file> <id> or <var> <id> when MD DEFDATA is set\n";
                    return false;
                }
                std::string recordBody;
                std::string hardError;
                const ProcLockOutcome outcome =
                    (*readUPtr)(target->file, target->id, recordBody, hardError);
                if (outcome == ProcLockOutcome::Locked) {
                    setProcLocked(variables);
                } else if (outcome == ProcLockOutcome::Success) {
                    variables[tokens[1]] = recordBody;
                    clearProcErr(variables);
                } else if (outcome == ProcLockOutcome::MissingRecord) {
                    out << "Error: No such record\n";
                    return false;
                } else {
                    out << "Error: " << hardError << "\n";
                    return false;
                }
                if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }
            if (parsed.command == Command::WriteU) {
                if (tokens.size() < 3) {
                    out << "Error: WRITEU requires file/record/value operands\n";
                    return false;
                }
                if (writeUPtr == nullptr) {
                    out << "Error: WRITEU not available in this PROC host\n";
                    return false;
                }
                const std::vector<std::string> operands =
                    substituteTokens(tokens, 1, variables, sessionPtr);
                std::optional<FileRecordPair> target;
                std::string value;
                if (operands.size() >= 3) {
                    target = FileRecordPair{operands[0], operands[1]};
                    value = joinTokens(operands, 2);
                } else if (operands.size() == 2) {
                    if (const std::optional<std::string> defData = defaultDataFile(sessionPtr)) {
                        target = FileRecordPair{*defData, operands[0]};
                        value = operands[1];
                    }
                }
                if (!target.has_value() || value.empty()) {
                    out << "Error: WRITEU requires <file> <id> <value> or <id> <value> when MD DEFDATA is set\n";
                    return false;
                }
                std::string hardError;
                const ProcLockOutcome outcome =
                    (*writeUPtr)(target->file, target->id, value, hardError);
                if (outcome == ProcLockOutcome::Locked) {
                    setProcLocked(variables);
                } else if (outcome == ProcLockOutcome::Success) {
                    clearProcErr(variables);
                } else {
                    out << "Error: " << hardError << "\n";
                    return false;
                }
                if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }
            if (parsed.command == Command::Release) {
                if (tokens.size() < 2) {
                    out << "Error: RELEASE requires file/record operands\n";
                    return false;
                }
                if (releasePtr == nullptr) {
                    out << "Error: RELEASE not available in this PROC host\n";
                    return false;
                }
                const std::vector<std::string> operands =
                    substituteTokens(tokens, 1, variables, sessionPtr);
                const std::optional<FileRecordPair> target = parseFileRecordPair(operands, sessionPtr);
                if (!target.has_value()) {
                    out << "Error: RELEASE requires <file> <id> or <id> when MD DEFDATA is set\n";
                    return false;
                }
                std::string hardError;
                if (!(*releasePtr)(target->file, target->id, hardError)) {
                    out << "Error: " << hardError << "\n";
                    return false;
                }
                clearProcErr(variables);
                if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }
            if (parsed.command == Command::Assignment) {
                if (isReadonlySessionAtTarget(tokens[0])) {
                    out << "Error: Read-only system variable\n";
                    return false;
                }
                variables[tokens[0]] = joinTokens(substituteTokens(tokens, 2, variables, sessionPtr), 0);
                if (!inlineStmt) {
                    nextLine();
                }
                return true;
            }

            out << "Error: Unknown PROC statement\n";
            return false;
        };

        while (pc >= 0 && pc < static_cast<int>(lines.size())) {
            const std::string rawLine = trim(lines[static_cast<std::size_t>(pc)]);
            if (rawLine.empty() || rawLine.back() == ':') {
                ++pc;
                continue;
            }
            syncLoopFramesForPc(pc);
            const std::vector<std::string> tokens = lineTokens[static_cast<std::size_t>(pc)];
            if (tokens.empty()) {
                ++pc;
                continue;
            }
            if (!execStatement(tokens, pc, false)) {
                return false;
            }
        }
        return true;
    }
} // namespace PickShell
