#include "BasicProgram.h"

#include <string>
#include <utility>

namespace PickShell {
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
        std::map<int, std::string> renumbered;
        int next = first;
        for (const auto &[_, text]: lines_) {
            renumbered[next] = text;
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
