#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "LoginService.h"

static std::filesystem::path uniqueTempDir() {
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("pick-login-test-" + std::to_string(tick));
}

TEST_CASE("LoginService authenticate succeeds with dev password hash") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    std::filesystem::create_directories(gem / "accounts" / "TST" / "VOC");
    {
        std::ofstream users(gem / "USERS.json");
        users << R"({"users":[{"username":"u1","passwordHash":"dev-x","defaultAccount":"TST","privileges":""}]})";
    }
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"}]})";
    }

    std::ostringstream err;
    const auto s = PickCore::LoginService::authenticate(gem, "u1", "", err);
    REQUIRE(s.has_value());
    CHECK(s->username == "u1");
    CHECK(s->accountName == "TST");
    CHECK(s->pickRoot == (gem / "accounts/TST").lexically_normal());
    CHECK(err.str().empty());
}

TEST_CASE("LoginService authenticate rejects unknown user") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    std::filesystem::create_directories(gem / "accounts" / "TST" / "VOC");
    {
        std::ofstream users(gem / "USERS.json");
        users << R"({"users":[{"username":"u1","passwordHash":"dev-x","defaultAccount":"TST","privileges":""}]})";
    }
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"}]})";
    }

    std::ostringstream err;
    const auto s = PickCore::LoginService::authenticate(gem, "nobody", "", err);
    CHECK_FALSE(s.has_value());
    CHECK(err.str().find("Unknown user") != std::string::npos);
}
