#ifndef GEMINI_MATH_FUNCTION_IDS_HPP
#define GEMINI_MATH_FUNCTION_IDS_HPP

#include <cstdint>

namespace Gemini::Math {
    using FunctionId = std::uint32_t;

    /// Unary real→real; negative → MATH: SQRT domain (Stage 2).
    constexpr FunctionId kFnSqrt = 0;

    /// Unary real→real; radians.
    constexpr FunctionId kFnSin = 1;

    /// Unary real→real; radians.
    constexpr FunctionId kFnCos = 2;

    /// Unary real→real; radians.
    constexpr FunctionId kFnTan = 3;

    /// Unary real→real; radians (`std::atan`).
    constexpr FunctionId kFnArctan = 4;

    /// Unary real→real natural log; ≤0 → MATH: LN domain (Stage 2).
    constexpr FunctionId kFnLn = 5;

    /// Unary real→real.
    constexpr FunctionId kFnExp = 6;

    constexpr FunctionId kFunctionCount = 7;
} // namespace Gemini::Math

#endif // GEMINI_MATH_FUNCTION_IDS_HPP
