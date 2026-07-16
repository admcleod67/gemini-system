#ifndef GEMINI_PASCAL_FUNCTION_IDS_HPP
#define GEMINI_PASCAL_FUNCTION_IDS_HPP

#include <cstdint>

namespace Gemini::Pascal {
    using FunctionId = std::uint32_t;

    /// Pop one printable value; write to stdout without newline.
    constexpr FunctionId kFnWrite = 0;

    /// Pop one printable value; write to stdout and append newline.
    constexpr FunctionId kFnWriteln = 1;

    /// Write newline only (bare `writeln;`).
    constexpr FunctionId kFnWriteln0 = 2;

    /// Pop variable name string; read from stdin into that variable via Runtime (hostContext).
    constexpr FunctionId kFnRead = 3;

    /// Pop variable name string; read line from stdin into that variable via Runtime (hostContext).
    constexpr FunctionId kFnReadln = 4;

    constexpr FunctionId kFunctionCount = 5;
} // namespace Gemini::Pascal

#endif // GEMINI_PASCAL_FUNCTION_IDS_HPP
