#ifndef PICK_SYSTEM_CORE_CONVERSION_PICK_OCONV_H
#define PICK_SYSTEM_CORE_CONVERSION_PICK_OCONV_H

#include <optional>
#include <string>

namespace PickCore::Conversion {
    /// Milestone 7 OCONV output conversion (shared by BASIC builtins and F-type correlatives).
    /// @param value Raw cell value (coerced numerically when needed).
    /// @param codeRaw Conversion code (trimmed and uppercased internally).
    /// @param error Set to `F-type: unsupported conversion code "…"` on failure when using correlatives.
    /// @return Converted string, or std::nullopt on unsupported code / invalid input for correlatives path.
    [[nodiscard]] std::optional<std::string> oconvOutput(const std::string &value,
                                                         const std::string &codeRaw,
                                                         std::string &error);

    /// Same table as `oconvOutput`, for BASIC `OCONV` builtin — throws `std::runtime_error` with
    /// `BUILTIN: OCONV unsupported code "…"` on unknown codes.
    [[nodiscard]] std::string oconvOutputBuiltin(const std::string &value, const std::string &codeRaw);
} // namespace PickCore::Conversion

#endif
