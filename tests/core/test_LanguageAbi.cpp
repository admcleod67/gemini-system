#include <doctest/doctest.h>

#include "BasicLanguageIds.h"

#include <gemini/basic_function_ids.hpp>
#include <gemini/language_abi.hpp>
#include <gemini/namespace_ids.hpp>

TEST_CASE("published Gemini ABI matches internal Basic language IDs") {
    CHECK(Gemini::kLanguageAbiVersion == 1);
    CHECK(Gemini::kNamespaceIdBasic == PickCore::Languages::Basic::kNamespaceId);
    CHECK(Gemini::Basic::kFnLen == PickCore::Languages::Basic::kFnLen);
    CHECK(Gemini::Basic::kFnRnd0 == PickCore::Languages::Basic::kFnRnd0);
    CHECK(Gemini::Basic::kFnRnd1 == PickCore::Languages::Basic::kFnRnd1);
    CHECK(Gemini::Basic::kFunctionCount == PickCore::Languages::Basic::kFunctionCount);
    CHECK(Gemini::kNamespaceIdPascal == 0x00000003U);
    CHECK(Gemini::kNamespaceIdComal == 0x00000004U);
    CHECK(Gemini::kNamespaceIdCobol == 0x00000005U);
    CHECK(Gemini::kNamespaceIdStub == 0x00000100U);
}
