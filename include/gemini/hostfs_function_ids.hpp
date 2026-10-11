#ifndef GEMINI_HOSTFS_FUNCTION_IDS_HPP
#define GEMINI_HOSTFS_FUNCTION_IDS_HPP

#include <cstdint>

namespace Gemini::HostFs {
    using FunctionId = std::uint32_t;

    /// Open path for sequential read; push handle int. Empty path → HOSTFS: invalid path.
    constexpr FunctionId kFnOpenRead = 0;

    /// Open path for write (create/truncate); push handle int.
    constexpr FunctionId kFnOpenWrite = 1;

    /// Open path for append (create if missing); push handle int.
    constexpr FunctionId kFnOpenAppend = 2;

    /// Read up to maxBytes from handle; push string (empty at EOF). Arity 2: handle, maxBytes.
    constexpr FunctionId kFnRead = 3;

    /// Write data string to handle; push int bytes written. Arity 2: handle, data.
    constexpr FunctionId kFnWrite = 4;

    /// Close handle; push int 0.
    constexpr FunctionId kFnClose = 5;

    constexpr FunctionId kFunctionCount = 6;
} // namespace Gemini::HostFs

#endif // GEMINI_HOSTFS_FUNCTION_IDS_HPP
