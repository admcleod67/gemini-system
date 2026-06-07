#ifndef PICK_SYSTEM_CORE_CONVERSION_PICK_ICONV_H
#define PICK_SYSTEM_CORE_CONVERSION_PICK_ICONV_H

#include <optional>
#include <string>

namespace PickCore::Conversion {
    /// Milestone 7 ICONV input conversion (shared by BASIC builtins and I-type correlatives).
    /// @return Converted value as string (numeric codes stringify integers).
    [[nodiscard]] std::optional<std::string> iconvOutput(const std::string &value,
                                                         const std::string &codeRaw,
                                                         std::string &error);

    /// Same table as `iconvOutput`, for BASIC `ICONV` builtin — throws on failure.
    [[nodiscard]] std::string iconvOutputBuiltin(const std::string &value, const std::string &codeRaw);
} // namespace PickCore::Conversion

#endif
