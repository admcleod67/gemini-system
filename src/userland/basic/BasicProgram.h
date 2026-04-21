//
// BASIC program buffer used by BASIC mode.
//

#ifndef PICK_SYSTEM_BASIC_BASICPROGRAM_H
#define PICK_SYSTEM_BASIC_BASICPROGRAM_H

#pragma once

#include <map>
#include <optional>
#include <ostream>
#include <string>

namespace PickShell {
    class BasicProgram {
    public:
        void setName(std::optional<std::string> name);

        [[nodiscard]] const std::optional<std::string> &name() const { return name_; }

        void clearLines();

        [[nodiscard]] bool empty() const { return lines_.empty(); }

        void setLine(int lineNumber, std::string text);

        [[nodiscard]] bool hasLine(int lineNumber) const;

        [[nodiscard]] std::optional<std::string> lineText(int lineNumber) const;

        [[nodiscard]] bool deleteLine(int lineNumber);

        [[nodiscard]] std::size_t deleteRange(int startLine, int endLine);

        void renumber(int first = 10, int increment = 10);

        void list(std::ostream &out) const;

        [[nodiscard]] const std::map<int, std::string> &lines() const { return lines_; }

    private:
        std::optional<std::string> name_;
        std::map<int, std::string> lines_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_BASIC_BASICPROGRAM_H
