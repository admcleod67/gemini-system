#ifndef GEMINI_NAMESPACE_IDS_HPP
#define GEMINI_NAMESPACE_IDS_HPP

#include <cstdint>

namespace Gemini {
    using NamespaceId = std::uint32_t;

    /// ABI version for published namespace/function ID tables (Milestone 11 Stage 5).
    constexpr int kLanguageAbiVersion = 1;

    /// Reserved for in-process unit tests; not shipped in bootstrap modules.
    constexpr NamespaceId kNamespaceIdReservedTest = 0x00000001;

    /// BASIC built-ins (reference module: gemini-module-basic).
    constexpr NamespaceId kNamespaceIdBasic = 0x00000002;

    /// Pascal helper library (console function IDs published; handlers are follow-on work).
    constexpr NamespaceId kNamespaceIdPascal = 0x00000003;

    /// COMAL helper library (stub until a COMAL compiler ships).
    constexpr NamespaceId kNamespaceIdComal = 0x00000004;

    /// COBOL helper library (stub until a COBOL compiler ships).
    constexpr NamespaceId kNamespaceIdCobol = 0x00000005;

    /// Integration-test stub module (gemini-module-stub).
    constexpr NamespaceId kNamespaceIdStub = 0x00000100;
} // namespace Gemini

#endif // GEMINI_NAMESPACE_IDS_HPP
