#include <doctest/doctest.h>

#include "TclEnvironment.h"

TEST_CASE("TclEnvironment set get and overwrite") {
    PickShell::TclEnvironment env;
    CHECK(env.set("a", "1"));
    REQUIRE(env.get("a"));
    CHECK(*env.get("a") == "1");
    CHECK(env.set("a", "2"));
    REQUIRE(env.get("a"));
    CHECK(*env.get("a") == "2");
}

TEST_CASE("TclEnvironment set lower then upper same key") {
    PickShell::TclEnvironment env;
    CHECK(env.set("a", "1"));
    CHECK(env.set("A", "2"));
    REQUIRE(env.get("a"));
    CHECK(*env.get("a") == "2");
    CHECK(env.names().size() == 1);
}

TEST_CASE("TclEnvironment get missing returns nullopt") {
    PickShell::TclEnvironment env;
    CHECK_FALSE(env.get("missing").has_value());
}

TEST_CASE("TclEnvironment get case insensitive") {
    PickShell::TclEnvironment env;
    CHECK(env.set("greeting", "hi"));
    REQUIRE(env.get("GREETING"));
    CHECK(*env.get("GREETING") == "hi");
}

TEST_CASE("TclEnvironment set rejects empty name") {
    PickShell::TclEnvironment env;
    CHECK_FALSE(env.set("", "x"));
}

TEST_CASE("TclEnvironment unset") {
    PickShell::TclEnvironment env;
    CHECK(env.set("x", "y"));
    CHECK(env.unset("x"));
    CHECK_FALSE(env.get("x").has_value());
    CHECK_FALSE(env.unset("x"));
}

TEST_CASE("TclEnvironment names sorted") {
    PickShell::TclEnvironment env;
    CHECK(env.set("z", "1"));
    CHECK(env.set("a", "2"));
    CHECK(env.set("m", "3"));
    const std::vector<std::string> n = env.names();
    REQUIRE(n.size() == 3);
    CHECK(n[0] == "A");
    CHECK(n[1] == "M");
    CHECK(n[2] == "Z");
}

TEST_CASE("TclEnvironment canonicalVariableName") {
    CHECK(PickShell::TclEnvironment::canonicalVariableName("foo") == "FOO");
    CHECK(PickShell::TclEnvironment::canonicalVariableName("BAR") == "BAR");
}

TEST_CASE("TclEnvironment clear") {
    PickShell::TclEnvironment env;
    CHECK(env.set("p", "q"));
    env.clear();
    CHECK_FALSE(env.get("p").has_value());
    CHECK(env.names().empty());
}
