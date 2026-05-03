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

TEST_CASE("LoginService authenticateAccount succeeds without account password") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    std::filesystem::create_directories(gem / "accounts" / "TST" / "VOC");
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"}]})";
    }

    std::ostringstream err;
    const auto s = PickCore::LoginService::authenticateAccount(gem, "TST", "", err);
    REQUIRE(s.has_value());
    CHECK(s->username == "TST");
    CHECK(s->accountName == "TST");
    CHECK(s->pickRoot == (gem / "accounts/TST").lexically_normal());
    CHECK(err.str().empty());
}

TEST_CASE("LoginService authenticateAccount rejects unknown account") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    std::filesystem::create_directories(gem / "accounts" / "TST" / "VOC");
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"}]})";
    }

    std::ostringstream err;
    const auto s = PickCore::LoginService::authenticateAccount(gem, "OTHER", "", err);
    CHECK_FALSE(s.has_value());
    CHECK(err.str().find("Unknown account") != std::string::npos);
}

TEST_CASE("LoginService authenticateAccount optional passwordHash") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    std::filesystem::create_directories(gem / "accounts" / "X" / "VOC");
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"X","root":"accounts/X","passwordHash":"secret"}]})";
    }

    std::ostringstream errBad;
    CHECK_FALSE(PickCore::LoginService::authenticateAccount(gem, "X", "", errBad).has_value());

    std::ostringstream errOk;
    const auto s = PickCore::LoginService::authenticateAccount(gem, "X", "secret", errOk);
    REQUIRE(s.has_value());
    CHECK(errOk.str().empty());
}

TEST_CASE("LoginService runCatalogLogin prints LOGON PLEASE with MD auto account") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pick = gem / "accounts" / "TST";
    std::filesystem::create_directories(pick / "MD");
    std::filesystem::create_directories(pick / "VOC");
    {
        std::ofstream md(pick / "MD" / "AUTO-LOGON.item");
        md << "TST\n";
    }
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"}]})";
    }

    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto s = PickCore::LoginService::runCatalogLogin(in, out, gem, pick, &err);
    REQUIRE(s.has_value());
    CHECK(s->accountName == "TST");
    CHECK(out.str() == "LOGON PLEASE: TST\n");
}

TEST_CASE("LoginService runCatalogLogin falls back interactive when MD token invalid") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pick = gem / "accounts" / "TST";
    std::filesystem::create_directories(pick / "MD");
    std::filesystem::create_directories(pick / "VOC");
    {
        std::ofstream md(pick / "MD" / "AUTO-LOGON.item");
        md << "HELP\n"; // reserved
    }
    {
        std::ofstream accounts(gem / "ACCOUNTS.json");
        accounts << R"({"accounts":[{"name":"TST","root":"accounts/TST"}]})";
    }

    std::istringstream in("TST\n");
    std::ostringstream out;
    const auto s = PickCore::LoginService::runCatalogLogin(in, out, gem, pick, nullptr);
    REQUIRE(s.has_value());
    CHECK(out.str().find("LOGON PLEASE:\n") != std::string::npos);
}
