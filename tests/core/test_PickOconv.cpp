#include <doctest/doctest.h>

#include "PickOconv.h"

TEST_CASE("PickOconv D formats Pick day 732 as 01 Jan 1970") {
    std::string error;
    const auto out = PickCore::Conversion::oconvOutput("732", "D", error);
    REQUIRE(out.has_value());
    CHECK(error.empty());
    CHECK(*out == "01 Jan 1970");
}

TEST_CASE("PickOconv MD2 formats scaled integer") {
    std::string error;
    const auto out = PickCore::Conversion::oconvOutput("1234", "MD2", error);
    REQUIRE(out.has_value());
    CHECK(*out == "12.34");
}

TEST_CASE("PickOconv builtin path matches oconvOutput for D") {
    CHECK(PickCore::Conversion::oconvOutputBuiltin("732", "D") == "01 Jan 1970");
}

TEST_CASE("PickOconv rejects unsupported code with F-type message") {
    std::string error;
    CHECK_FALSE(PickCore::Conversion::oconvOutput("1", "XY", error).has_value());
    CHECK(error == "F-type: unsupported conversion code \"XY\"");
}
