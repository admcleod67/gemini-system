#ifndef PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_LOOKUP_H
#define PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_LOOKUP_H

#include "BasicLanguageIds.h"

#include <optional>
#include <string_view>

namespace PickCore::Languages::Basic {
    [[nodiscard]] inline std::optional<FunctionId> functionIdForCall(const std::string_view name, const int argCount) {
        if (name == "ABS" && argCount == 1) {
            return kFnAbs;
        }
        if (name == "SGN" && argCount == 1) {
            return kFnSgn;
        }
        if (name == "SEQ" && argCount == 1) {
            return kFnSeq;
        }
        if (name == "LEN" && argCount == 1) {
            return kFnLen;
        }
        if (name == "TRIM" && argCount == 1) {
            return kFnTrim;
        }
        if (name == "LCASE" && argCount == 1) {
            return kFnLcase;
        }
        if (name == "UCASE" && argCount == 1) {
            return kFnUcase;
        }
        if (name == "SPACE" && argCount == 1) {
            return kFnSpace;
        }
        if (name == "INT" && argCount == 1) {
            return kFnInt;
        }
        if (name == "MOD" && argCount == 2) {
            return kFnMod;
        }
        if (name == "SIN" && argCount == 1) {
            return kFnSin;
        }
        if (name == "COS" && argCount == 1) {
            return kFnCos;
        }
        if (name == "TAN" && argCount == 1) {
            return kFnTan;
        }
        if (name == "EXP" && argCount == 1) {
            return kFnExp;
        }
        if (name == "LOG" && argCount == 1) {
            return kFnLog;
        }
        if (name == "DATE" && argCount == 0) {
            return kFnDate;
        }
        if (name == "TIME" && argCount == 0) {
            return kFnTime;
        }
        if (name == "SYSTEM" && argCount == 1) {
            return kFnSystem;
        }
        if (name == "INDEX" && argCount == 3) {
            return kFnIndex;
        }
        if (name == "FIELD" && argCount == 3) {
            return kFnField;
        }
        if (name == "STR" && argCount == 1) {
            return kFnStr;
        }
        if (name == "OCONV" && argCount == 2) {
            return kFnOconv;
        }
        if (name == "ICONV" && argCount == 2) {
            return kFnIconv;
        }
        if (name == "NUM" && argCount == 1) {
            return kFnNum;
        }
        if (name == "CONVERT" && argCount == 3) {
            return kFnConvert;
        }
        if (name == "RND") {
            if (argCount == 0) {
                return kFnRnd0;
            }
            if (argCount == 1) {
                return kFnRnd1;
            }
            return std::nullopt;
        }
        if (name == "STATUS" && argCount == 0) {
            return kFnStatus;
        }
        return std::nullopt;
    }
} // namespace PickCore::Languages::Basic

#endif // PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_LOOKUP_H
