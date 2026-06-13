#ifndef PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_LANGUAGE_IDS_H
#define PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_LANGUAGE_IDS_H

#include "../LanguageTypes.h"

#include <gemini/basic_function_ids.hpp>
#include <gemini/namespace_ids.hpp>

namespace PickCore::Languages::Basic {
    constexpr NamespaceId kNamespaceId = Gemini::kNamespaceIdBasic;

    constexpr FunctionId kFnAbs = Gemini::Basic::kFnAbs;
    constexpr FunctionId kFnSgn = Gemini::Basic::kFnSgn;
    constexpr FunctionId kFnSeq = Gemini::Basic::kFnSeq;
    constexpr FunctionId kFnLen = Gemini::Basic::kFnLen;
    constexpr FunctionId kFnTrim = Gemini::Basic::kFnTrim;
    constexpr FunctionId kFnLcase = Gemini::Basic::kFnLcase;
    constexpr FunctionId kFnUcase = Gemini::Basic::kFnUcase;
    constexpr FunctionId kFnSpace = Gemini::Basic::kFnSpace;
    constexpr FunctionId kFnInt = Gemini::Basic::kFnInt;
    constexpr FunctionId kFnMod = Gemini::Basic::kFnMod;
    constexpr FunctionId kFnSin = Gemini::Basic::kFnSin;
    constexpr FunctionId kFnCos = Gemini::Basic::kFnCos;
    constexpr FunctionId kFnTan = Gemini::Basic::kFnTan;
    constexpr FunctionId kFnExp = Gemini::Basic::kFnExp;
    constexpr FunctionId kFnLog = Gemini::Basic::kFnLog;
    constexpr FunctionId kFnDate = Gemini::Basic::kFnDate;
    constexpr FunctionId kFnTime = Gemini::Basic::kFnTime;
    constexpr FunctionId kFnSystem = Gemini::Basic::kFnSystem;
    constexpr FunctionId kFnIndex = Gemini::Basic::kFnIndex;
    constexpr FunctionId kFnField = Gemini::Basic::kFnField;
    constexpr FunctionId kFnStr = Gemini::Basic::kFnStr;
    constexpr FunctionId kFnOconv = Gemini::Basic::kFnOconv;
    constexpr FunctionId kFnIconv = Gemini::Basic::kFnIconv;
    constexpr FunctionId kFnNum = Gemini::Basic::kFnNum;
    constexpr FunctionId kFnConvert = Gemini::Basic::kFnConvert;
    constexpr FunctionId kFnRnd0 = Gemini::Basic::kFnRnd0;
    constexpr FunctionId kFnRnd1 = Gemini::Basic::kFnRnd1;
    constexpr FunctionId kFnStatus = Gemini::Basic::kFnStatus;

    constexpr FunctionId kFunctionCount = Gemini::Basic::kFunctionCount;

    static_assert(kNamespaceId == Gemini::kNamespaceIdBasic);
    static_assert(kFnAbs == Gemini::Basic::kFnAbs);
    static_assert(kFnStatus == Gemini::Basic::kFnStatus);
    static_assert(kFunctionCount == Gemini::Basic::kFunctionCount);
} // namespace PickCore::Languages::Basic

#endif // PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_LANGUAGE_IDS_H
