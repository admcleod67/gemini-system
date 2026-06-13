#ifndef GEMINI_BASIC_FUNCTION_IDS_HPP
#define GEMINI_BASIC_FUNCTION_IDS_HPP

#include <cstdint>

namespace Gemini::Basic {
    using FunctionId = std::uint32_t;

    constexpr FunctionId kFnAbs = 0;
    constexpr FunctionId kFnSgn = 1;
    constexpr FunctionId kFnSeq = 2;
    constexpr FunctionId kFnLen = 3;
    constexpr FunctionId kFnTrim = 4;
    constexpr FunctionId kFnLcase = 5;
    constexpr FunctionId kFnUcase = 6;
    constexpr FunctionId kFnSpace = 7;
    constexpr FunctionId kFnInt = 8;
    constexpr FunctionId kFnMod = 9;
    constexpr FunctionId kFnSin = 10;
    constexpr FunctionId kFnCos = 11;
    constexpr FunctionId kFnTan = 12;
    constexpr FunctionId kFnExp = 13;
    constexpr FunctionId kFnLog = 14;
    constexpr FunctionId kFnDate = 15;
    constexpr FunctionId kFnTime = 16;
    constexpr FunctionId kFnSystem = 17;
    constexpr FunctionId kFnIndex = 18;
    constexpr FunctionId kFnField = 19;
    constexpr FunctionId kFnStr = 20;
    constexpr FunctionId kFnOconv = 21;
    constexpr FunctionId kFnIconv = 22;
    constexpr FunctionId kFnNum = 23;
    constexpr FunctionId kFnConvert = 24;
    constexpr FunctionId kFnRnd0 = 25;
    constexpr FunctionId kFnRnd1 = 26;
    constexpr FunctionId kFnStatus = 27;

    constexpr FunctionId kFunctionCount = 28;
} // namespace Gemini::Basic

#endif // GEMINI_BASIC_FUNCTION_IDS_HPP
