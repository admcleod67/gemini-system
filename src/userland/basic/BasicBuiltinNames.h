#ifndef PICK_SYSTEM_BASIC_BASICBUILTINNAMES_H
#define PICK_SYSTEM_BASIC_BASICBUILTINNAMES_H

#pragma once

#include <optional>
#include <string_view>
#include <utility>

namespace PickShell::BasicBuiltins {

    [[nodiscard]] inline bool isBuiltinCallName(const std::string_view upper) {
        return upper == "ABS" || upper == "SGN" || upper == "SEQ" || upper == "LEN" || upper == "TRIM" ||
               upper == "LCASE" || upper == "UCASE" || upper == "SPACE" || upper == "INT" || upper == "MOD" ||
               upper == "RND" || upper == "SIN" || upper == "COS" || upper == "TAN" || upper == "EXP" ||
               upper == "LOG" || upper == "DATE" || upper == "TIME" || upper == "SYSTEM";
    }

    /// Inclusive (minArgs, maxArgs) for whitelisted builtin calls; std::nullopt if not a builtin call name.
    [[nodiscard]] inline std::optional<std::pair<int, int>> builtinArityBounds(const std::string_view upper) {
        if (upper == "DATE" || upper == "TIME") {
            return std::make_pair(0, 0);
        }
        if (upper == "RND") {
            return std::make_pair(0, 1);
        }
        if (upper == "MOD") {
            return std::make_pair(2, 2);
        }
        if (upper == "ABS" || upper == "SGN" || upper == "SEQ" || upper == "LEN" || upper == "TRIM" ||
            upper == "LCASE" || upper == "UCASE" || upper == "SPACE" || upper == "INT" || upper == "SIN" ||
            upper == "COS" || upper == "TAN" || upper == "EXP" || upper == "LOG" || upper == "SYSTEM") {
            return std::make_pair(1, 1);
        }
        return std::nullopt;
    }

} // namespace PickShell::BasicBuiltins

#endif // PICK_SYSTEM_BASIC_BASICBUILTINNAMES_H
