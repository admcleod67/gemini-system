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
    CHECK(s.find("CATALOG ATTACHED") != std::string::npos);
    CHECK(s.find("PORT MANAGER:") != std::string::npos);
    CHECK(s.find("SYSTEM READY") != std::string::npos);
    CHECK(s.find("INITIALIZING") < s.find("MD ATTACHED"));
    CHECK(s.find("MD ATTACHED") < s.find("SYSTEM READY"));
}
