#ifndef PICK_SYSTEM_BASIC_BASICBUILTINNAMES_H
#define PICK_SYSTEM_BASIC_BASICBUILTINNAMES_H

#pragma once

#include <optional>
#include <string_view>

namespace PickShell::BasicBuiltins {

    [[nodiscard]] inline bool isBuiltinCallName(const std::string_view upper) {
        return upper == "ABS" || upper == "SGN" || upper == "SEQ";
    }

    /// Arity for whitelisted builtin calls; std::nullopt if not a builtin call name.
    [[nodiscard]] inline std::optional<int> arityForBuiltinCall(const std::string_view upper) {
        if (upper == "ABS" || upper == "SGN" || upper == "SEQ") {
            return 1;
        }
        return std::nullopt;
    }

} // namespace PickShell::BasicBuiltins

#endif // PICK_SYSTEM_BASIC_BASICBUILTINNAMES_H
