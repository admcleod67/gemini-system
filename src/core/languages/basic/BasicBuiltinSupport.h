#ifndef PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_SUPPORT_H
#define PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_SUPPORT_H

#include "Runtime.h"

#include <string>

namespace PickCore::Languages::Basic {
    constexpr int kPickInternalDateAtUnixEpoch = 732;
    constexpr int kBuiltinSpaceMaxCount = 65536;

    [[nodiscard]] double coerceToDouble(const PickVM::Value &v);
    [[nodiscard]] int coerceToInt(const PickVM::Value &v);
    [[nodiscard]] std::string valueToString(const PickVM::Value &v);
    [[nodiscard]] bool isFullyNumeric(const std::string &s);

    [[nodiscard]] int daysFromCivil(int y, int m, int d);
    [[nodiscard]] std::string dateStringFromPickDay(int day);
    [[nodiscard]] int pickDayFromDateString(const std::string &s, bool &ok);
    [[nodiscard]] std::string timeStringFromSeconds(int n, bool wantSeconds);
    [[nodiscard]] int secondsFromTimeString(const std::string &s, bool wantSeconds, bool &ok);
    [[nodiscard]] std::string formatMdScaled(long long intVal, int decimals);
    [[nodiscard]] long long parseMdScaled(const std::string &s, int decimals, bool &ok);

    enum class ConvertDirection { Output, Input };

    [[nodiscard]] PickVM::Value dispatchConvertCode(ConvertDirection dir,
                                                    const PickVM::Value &value,
                                                    const std::string &codeRaw);
} // namespace PickCore::Languages::Basic

#endif // PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_SUPPORT_H
