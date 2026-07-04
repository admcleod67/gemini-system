#include <doctest/doctest.h>

#include <pick_system/version.hpp>

#include "GeminiSession.h"

#include <sstream>
#include <string>

TEST_CASE("GeminiSession constructs and exposes stable runtime") {
    PickShell::GeminiSession session;
    PickVM::Runtime *const runtimeAddr = &session.runtime();
    CHECK(runtimeAddr == &session.runtime());
}

TEST_CASE("GeminiSession shell VERSION") {
    PickShell::GeminiSession session;
    std::ostringstream out;
    bool quit = false;
    session.shell().handleLine("VERSION", out, quit);
    CHECK_FALSE(quit);
    CHECK(out.str().find(pick_system::version_string) != std::string::npos);
}
