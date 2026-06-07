#include <doctest/doctest.h>

#include "PickIconv.h"

TEST_CASE("PickIconv D converts date string to Pick day") {
    std::string error;
    const auto out = PickCore::Conversion::iconvOutput("01 Jan 1970", "D", error);
    REQUIRE(out.has_value());
    CHECK(error.empty());
    CHECK(*out == "732");
}

TEST_CASE("PickIconv MD2 parses scaled decimal") {
    std::string error;
    const auto out = PickCore::Conversion::iconvOutput("12.34", "MD2", error);
    REQUIRE(out.has_value());
    CHECK(*out == "1234");
}

TEST_CASE("PickIconv builtin path matches iconvOutput for D") {
    CHECK(PickCore::Conversion::iconvOutputBuiltin("01 Jan 1970", "D") == "732");
}

TEST_CASE("PickIconv rejects unsupported code") {
    std::string error;
    CHECK_FALSE(PickCore::Conversion::iconvOutput("1", "XY", error).has_value());
    CHECK(error == "I-type: unsupported conversion code \"XY\"");
}

TEST_CASE("PickIconv rejects invalid D input") {
    std::string error;
    CHECK_FALSE(PickCore::Conversion::iconvOutput("not-a-date", "D", error).has_value());
    CHECK(error == "I-type: invalid ICONV \"D\" input");
}
