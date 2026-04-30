#include "LineRecordEditor.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <iostream>
#include <limits>
#include <sstream>

namespace PickShell {
    namespace {
        std::string joinLines(const std::vector<std::string> &lines) {
            std::string payload;
            for (std::size_t i = 0; i < lines.size(); ++i) {
                if (i > 0) {
                    payload.push_back('\n');
                }
                payload += lines[i];
            }
            return payload;
        }
    } // namespace

    LineRecordEditor::LineRecordEditor(PickFS::FileSystem &fileSystem,
                                       std::string fileName,
                                       std::string recordKey,
                                       std::vector<std::string> lines,
                                       PostWriteHook postWrite)
        : fileSystem_(fileSystem),
          fileName_(std::move(fileName)),
          recordKey_(std::move(recordKey)),
          lines_(std::move(lines)),
          postWrite_(std::move(postWrite)) {
    }

    std::vector<std::string> LineRecordEditor::tokenizeWs(const std::string &line) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string tok;
        while (iss >> tok) {
            tokens.push_back(std::move(tok));
        }
        return tokens;
    }

    std::string LineRecordEditor::trimAscii(const std::string &text) {
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

    std::string LineRecordEditor::upperAscii(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](const unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return text;
    }

    std::string LineRecordEditor::canonicalCommand(const std::string &token) {
        const std::string u = upperAscii(token);
        if (u == "L") {
            return "LIST";
        }
        if (u == "I") {
            return "INSERT";
        }
        if (u == "R") {
            return "REPLACE";
        }
        if (u == "D") {
            return "DELETE";
        }
        if (u == "FI") {
            return "SAVE";
        }
        if (u == "Q") {
            return "QUIT";
        }
        return u;
    }

    std::optional<int> LineRecordEditor::parsePositiveInt(const std::string &token) {
        try {
            const int value = std::stoi(token);
            if (value > 0) {
                return value;
            }
        } catch (const std::exception &) {
        }
        return std::nullopt;
    }

    bool LineRecordEditor::parseListRangeTokens(const std::vector<std::string> &tokens, int &startLine, int &endLine) {
        if (tokens.size() == 1) {
            startLine = 1;
            endLine = std::numeric_limits<int>::max();
            return true;
        }
        if (tokens.size() != 2) {
            return false;
        }
        const std::string &arg = tokens[1];
        const std::size_t dashPos = arg.find('-');
        if (dashPos == std::string::npos) {
            const std::optional<int> line = parsePositiveInt(arg);
            if (!line) {
                return false;
            }
            startLine = *line;
            endLine = *line;
            return true;
        }
        const std::string startToken = arg.substr(0, dashPos);
        const std::string endToken = arg.substr(dashPos + 1);
        const std::optional<int> start = parsePositiveInt(startToken);
        const std::optional<int> end = parsePositiveInt(endToken);
        if (!start || !end || *start > *end) {
            return false;
        }
        startLine = *start;
        endLine = *end;
        return true;
    }

    void LineRecordEditor::cmdList(const std::vector<std::string> &tokens, std::ostream &out) const {
        int startLine = 1;
        int endLine = std::numeric_limits<int>::max();
        if (!parseListRangeTokens(tokens, startLine, endLine)) {
            out << "LIST takes no args, LIST <line>, or LIST <start-end>\n";
            return;
        }
        const int maxLine = static_cast<int>(lines_.size());
        for (int i = 1; i <= maxLine; ++i) {
            if (i < startLine || i > endLine) {
                continue;
            }
            out << i;
            if (!lines_[static_cast<std::size_t>(i - 1)].empty()) {
                out << ' ' << lines_[static_cast<std::size_t>(i - 1)];
            }
            out << '\n';
        }
    }

    bool LineRecordEditor::readPeriodTerminatedBlock(std::istream &in,
                                                     std::vector<std::string> &outBlock,
                                                     std::ostream &out) {
        outBlock.clear();
        std::string line;
        while (std::getline(in, line)) {
            if (trimAscii(line) == ".") {
                return true;
            }
            outBlock.push_back(line);
        }
        out << "Unexpected end of input during INSERT/REPLACE\n";
        return false;
    }

    void LineRecordEditor::cmdInsert(std::istream &in, const std::vector<std::string> &tokens, std::ostream &out) {
        std::size_t insertIndex = lines_.size();
        if (tokens.size() > 2) {
            out << "INSERT takes no line number or one line number\n";
            return;
        }
        if (tokens.size() == 2) {
            const std::optional<int> afterLine = parsePositiveInt(tokens[1]);
            if (!afterLine) {
                out << "INSERT requires a positive line number\n";
                return;
            }
            if (*afterLine < 1 || *afterLine > static_cast<int>(lines_.size())) {
                out << "No such line\n";
                return;
            }
            insertIndex = static_cast<std::size_t>(*afterLine);
        }

        std::vector<std::string> block;
        if (!readPeriodTerminatedBlock(in, block, out)) {
            return;
        }
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(insertIndex), block.begin(), block.end());
    }

    void LineRecordEditor::cmdReplace(std::istream &in, const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 2) {
            out << "REPLACE requires a line number\n";
            return;
        }
        const std::optional<int> lineNo = parsePositiveInt(tokens[1]);
        if (!lineNo) {
            out << "REPLACE requires a line number\n";
            return;
        }
        if (*lineNo < 1 || *lineNo > static_cast<int>(lines_.size())) {
            out << "No such line\n";
            return;
        }

        std::vector<std::string> block;
        if (!readPeriodTerminatedBlock(in, block, out)) {
            return;
        }
        if (block.empty()) {
            out << "REPLACE requires at least one line before '.'\n";
            return;
        }

        const std::size_t idx = static_cast<std::size_t>(*lineNo - 1);
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(idx));
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(idx), block.begin(), block.end());
    }

    void LineRecordEditor::cmdDelete(const std::vector<std::string> &tokens, std::ostream &out) {
        if (tokens.size() != 2) {
            out << "DELETE requires a line number\n";
            return;
        }
        const std::optional<int> lineNo = parsePositiveInt(tokens[1]);
        if (!lineNo) {
            out << "DELETE requires a line number\n";
            return;
        }
        if (*lineNo < 1 || *lineNo > static_cast<int>(lines_.size())) {
            out << "No such line\n";
            return;
        }
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(*lineNo - 1));
    }

    bool LineRecordEditor::ensureFileExistsForWrite(std::ostream &out) {
        try {
            (void) fileSystem_.openFile(fileName_);
            return true;
        } catch (const PickFS::FileSystemError &openError) {
            if (std::string(openError.what()).find("File not found: ") != 0) {
                out << "Error: " << openError.what() << '\n';
                return false;
            }
            try {
                fileSystem_.createFile(fileName_);
                return true;
            } catch (const std::exception &e) {
                out << "Error: " << e.what() << '\n';
                return false;
            }
        }
    }

    void LineRecordEditor::cmdSave(std::ostream &out) {
        if (!ensureFileExistsForWrite(out)) {
            return;
        }
        try {
            fileSystem_.write(fileName_, PickFS::Record(recordKey_, joinLines(lines_)));
            if (postWrite_) {
                postWrite_(fileName_);
            }
        } catch (const std::exception &e) {
            out << "Error: " << e.what() << '\n';
        }
    }

    void LineRecordEditor::cmdHelp(std::ostream &out) const {
        out << "LIST (L) — list lines; optional LIST <line> or LIST <start-end>\n";
        out << "INSERT (I) — optional line number, then lines until '.' alone\n";
        out << "REPLACE (R) — line number, then lines until '.' alone\n";
        out << "DELETE (D) — line number\n";
        out << "SAVE (FI) — write buffer to record\n";
        out << "QUIT (Q) — exit without saving\n";
    }

    void LineRecordEditor::run(std::istream &in, std::ostream &out, std::optional<int> highlightPhysicalLine) {
        if (highlightPhysicalLine.has_value()) {
            const int row = *highlightPhysicalLine;
            if (row >= 1 && row <= static_cast<int>(lines_.size())) {
                const std::vector<std::string> listTok = {"LIST", std::to_string(row)};
                cmdList(listTok, out);
            }
        }
        while (true) {
            out << "ED> " << std::flush;
            std::string line;
            if (!std::getline(in, line)) {
                return;
            }
            const std::vector<std::string> tokens = tokenizeWs(line);
            if (tokens.empty()) {
                continue;
            }
            const std::string cmd = canonicalCommand(tokens[0]);
            if (cmd == "QUIT") {
                return;
            }
            if (cmd == "SAVE") {
                cmdSave(out);
                continue;
            }
            if (cmd == "HELP") {
                cmdHelp(out);
                continue;
            }
            if (cmd == "LIST") {
                cmdList(tokens, out);
                continue;
            }
            if (cmd == "INSERT") {
                cmdInsert(in, tokens, out);
                continue;
            }
            if (cmd == "REPLACE") {
                cmdReplace(in, tokens, out);
                continue;
            }
            if (cmd == "DELETE") {
                cmdDelete(tokens, out);
                continue;
            }
            out << "Unknown command: " << tokens[0] << '\n';
        }
    }
} // namespace PickShell
