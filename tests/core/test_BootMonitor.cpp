#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <pickvm/core.hpp>

#include "BootMonitor.h"

static std::filesystem::path uniqueTempDir() {
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("pick-boot-test-" + std::to_string(tick));
}

TEST_CASE("BootMonitor cold start prints ordered milestones") {
    PickVM::Runtime rt;
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pick = gem / "accounts" / "SY";
    std::filesystem::create_directories(pick / "MD");
    std::filesystem::create_directories(pick / "VOC");
    {
        std::ofstream acc(gem / "ACCOUNTS.json");
        acc << R"({"accounts":[{"name":"SY","root":"accounts/SY"}]})";
    }

    PickCore::BootContext ctx;
    ctx.runtime = &rt;
    ctx.hostPaths.pickFilesystemRoot = pick;
    ctx.hostPaths.geminiCatalogRoot = gem;

    std::ostringstream out;
    PickCore::BootMonitor::runColdStart(out, ctx);
    const std::string s = out.str();
    CHECK(s.find("INITIALIZING SYSTEM") != std::string::npos);
    CHECK(s.find("MEMORY:") != std::string::npos);
    CHECK(s.find("MD ATTACHED") != std::string::npos);
    CHECK(s.find("VOC ATTACHED") != std::string::npos);
    CHECK(s.find("CATALOG ATTACHED") != std::string::npos);
    CHECK(s.find("PORT MANAGER:") != std::string::npos);
    CHECK(s.find("SYSTEM READY\n\n") != std::string::npos);
    CHECK(s.find("INITIALIZING") < s.find("MD ATTACHED"));
    CHECK(s.find("MD ATTACHED") < s.find("SYSTEM READY"));
}

TEST_CASE("BootMonitor cold start reports missing and malformed catalogue diagnostics") {
    PickVM::Runtime rt;
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pick = gem / "accounts" / "SY";
    std::filesystem::create_directories(pick / "VOC");

    PickCore::BootContext ctx;
    ctx.runtime = &rt;
    ctx.hostPaths.pickFilesystemRoot = pick;
    ctx.hostPaths.geminiCatalogRoot = gem;

    std::ostringstream outMissing;
    PickCore::BootMonitor::runColdStart(outMissing, ctx);
    CHECK(outMissing.str().find("MD: (not present)") != std::string::npos);
    CHECK(outMissing.str().find("CATALOG: (missing ACCOUNTS.json)") != std::string::npos);

    {
        std::ofstream acc(gem / "ACCOUNTS.json");
        acc << R"({"accounts":)";
    }
    std::ostringstream outMalformed;
    PickCore::BootMonitor::runColdStart(outMalformed, ctx);
    CHECK(outMalformed.str().find("CATALOG: (malformed ACCOUNTS.json)") != std::string::npos);
}

TEST_CASE("BootMonitor cold start reports account root VOC and MD directory diagnostics") {
    PickVM::Runtime rt;
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    std::filesystem::create_directories(gem);
    {
        std::ofstream acc(gem / "ACCOUNTS.json");
        acc << R"({"accounts":[{"name":"A1","root":"accounts/A1"},{"name":"A2","root":"accounts/A2"}]})";
    }
    std::filesystem::create_directories(gem / "accounts" / "A2" / "VOC");

    PickCore::BootContext ctx;
    ctx.runtime = &rt;
    ctx.hostPaths.pickFilesystemRoot = gem / "accounts" / "A2";
    ctx.hostPaths.geminiCatalogRoot = gem;

    std::ostringstream out;
    PickCore::BootMonitor::runColdStart(out, ctx);
    const std::string s = out.str();
    CHECK(s.find("ACCOUNT A1: (root not present)") != std::string::npos);
    CHECK(s.find("ACCOUNT A2: (MD not present)") != std::string::npos);
}
