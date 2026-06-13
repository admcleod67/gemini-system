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

TEST_CASE("BootMonitor cold start loads language modules when modules path is set") {
#if !defined(GEMINI_STUB_MODULE_PATH)
    MESSAGE("GEMINI_STUB_MODULE_PATH not configured; skipping");
    return;
#endif
    const std::filesystem::path stubPath = GEMINI_STUB_MODULE_PATH;
    if (!std::filesystem::exists(stubPath)) {
        MESSAGE("stub module not built; skipping");
        return;
    }

    PickVM::Runtime rt;
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto modules = gem / "modules";
    std::filesystem::create_directories(modules);
    std::filesystem::copy_file(stubPath, modules / stubPath.filename());

    PickCore::BootContext ctx;
    ctx.runtime = &rt;
    ctx.hostPaths.geminiModulesRoot = modules;

    std::ostringstream out;
    PickCore::BootMonitor::runColdStart(out, ctx);
    const std::string s = out.str();
    CHECK(s.find("LOADING LANGUAGE MODULES") != std::string::npos);
    CHECK(s.find("MODULE ") != std::string::npos);
    CHECK(s.find(": OK") != std::string::npos);
    CHECK(s.find("MODULES: 1 loaded, 0 failed (1 attempted)") != std::string::npos);
}

TEST_CASE("BootMonitor cold start reports module load summary with failure") {
#if !defined(GEMINI_STUB_MODULE_PATH)
    MESSAGE("GEMINI_STUB_MODULE_PATH not configured; skipping");
    return;
#endif
    const std::filesystem::path stubPath = GEMINI_STUB_MODULE_PATH;
    if (!std::filesystem::exists(stubPath)) {
        MESSAGE("stub module not built; skipping");
        return;
    }

    PickVM::Runtime rt;
    const auto root = uniqueTempDir();
    const auto modules = root / "modules";
    std::filesystem::create_directories(modules);
    std::filesystem::copy_file(stubPath, modules / stubPath.filename());
    {
        std::ofstream bad(modules / "broken.so");
        bad << "not a shared library";
    }

    PickCore::BootContext ctx;
    ctx.runtime = &rt;
    ctx.hostPaths.geminiModulesRoot = modules;

    std::ostringstream out;
    PickCore::BootMonitor::runColdStart(out, ctx);
    const std::string s = out.str();
    CHECK(s.find("MODULES: 1 loaded, 1 failed (2 attempted)") != std::string::npos);
}
