#include <doctest/doctest.h>

#include "LanguageModuleLoader.h"
#include "Runtime.h"
#include "BasicLanguageIds.h"

#include <gemini/namespace_ids.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using PickCore::Languages::FunctionId;
using PickCore::Languages::LanguageModuleLoadReport;
using PickCore::Languages::LanguageRegistry;
using PickCore::Languages::NamespaceId;
using PickCore::Languages::loadLanguageModules;
using PickVM::Instruction;
using PickVM::OpCode;
using PickVM::Runtime;
using PickVM::Value;

namespace {
    constexpr NamespaceId kStubNamespaceId = Gemini::kNamespaceIdStub;
    constexpr FunctionId kStubFnNoOp = 0;

    std::filesystem::path uniqueTempDir() {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() / ("pick-lang-mod-" + std::to_string(tick));
    }

    std::filesystem::path stubModulePath() {
#if defined(GEMINI_STUB_MODULE_PATH)
        return std::filesystem::path(GEMINI_STUB_MODULE_PATH);
#else
        return {};
#endif
    }

    std::filesystem::path basicModulePath() {
#if defined(GEMINI_BASIC_MODULE_PATH)
        return std::filesystem::path(GEMINI_BASIC_MODULE_PATH);
#else
        return {};
#endif
    }

    std::filesystem::path pascalModulePath() {
#if defined(GEMINI_PASCAL_MODULE_PATH)
        return std::filesystem::path(GEMINI_PASCAL_MODULE_PATH);
#else
        return {};
#endif
    }

    std::filesystem::path comalModulePath() {
#if defined(GEMINI_COMAL_MODULE_PATH)
        return std::filesystem::path(GEMINI_COMAL_MODULE_PATH);
#else
        return {};
#endif
    }

    std::filesystem::path cobolModulePath() {
#if defined(GEMINI_COBOL_MODULE_PATH)
        return std::filesystem::path(GEMINI_COBOL_MODULE_PATH);
#else
        return {};
#endif
    }

    void copyModuleIfPresent(const std::filesystem::path &src, const std::filesystem::path &modulesDir) {
        if (!src.empty() && std::filesystem::exists(src)) {
            std::filesystem::copy_file(src, modulesDir / src.filename());
        }
    }

    constexpr NamespaceId kBasicNamespaceId = PickCore::Languages::Basic::kNamespaceId;

    Instruction makeCallFunc(const std::uint32_t namespaceId,
                             const std::uint32_t functionId,
                             const int argCount) {
        Instruction instr{};
        instr.op = OpCode::CallFunc;
        instr.callFunc = {namespaceId, functionId, argCount};
        return instr;
    }

    LanguageModuleLoadReport loadStubInto(LanguageRegistry &registry, const std::filesystem::path &modulesDir) {
        return loadLanguageModules(registry, modulesDir);
    }
} // namespace

TEST_CASE("LanguageModuleLoader loads built stub module") {
    const std::filesystem::path stubPath = stubModulePath();
    if (stubPath.empty() || !std::filesystem::exists(stubPath)) {
        MESSAGE("GEMINI_STUB_MODULE_PATH not configured; skipping");
        return;
    }

    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    std::filesystem::copy_file(stubPath, modulesDir / stubPath.filename());

    LanguageRegistry registry;
    const LanguageModuleLoadReport report = loadStubInto(registry, modulesDir);
    CHECK(report.attempted == 1);
    CHECK(report.loaded == 1);
    CHECK(report.failed == 0);
    CHECK(registry.isFrozen());

    const auto meta = registry.metadata(kStubNamespaceId);
    REQUIRE(meta.has_value());
    CHECK(meta->name == "stub");
    CHECK(meta->version == "1");
}

TEST_CASE("LanguageModuleLoader dispatches stub function after load") {
    const std::filesystem::path stubPath = stubModulePath();
    if (stubPath.empty() || !std::filesystem::exists(stubPath)) {
        MESSAGE("GEMINI_STUB_MODULE_PATH not configured; skipping");
        return;
    }

    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    std::filesystem::copy_file(stubPath, modulesDir / stubPath.filename());

    LanguageRegistry registry;
    loadStubInto(registry, modulesDir);

    std::vector<Value> stack;
    registry.dispatch(kStubNamespaceId, kStubFnNoOp, stack, 0);
    REQUIRE(stack.size() == 1);
    CHECK(std::get<std::string>(stack.back()) == "stub-ok");
}

TEST_CASE("LanguageModuleLoader CALL_FUNC end-to-end with loaded registry") {
    const std::filesystem::path stubPath = stubModulePath();
    if (stubPath.empty() || !std::filesystem::exists(stubPath)) {
        MESSAGE("GEMINI_STUB_MODULE_PATH not configured; skipping");
        return;
    }

    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    std::filesystem::copy_file(stubPath, modulesDir / stubPath.filename());

    LanguageRegistry registry;
    loadStubInto(registry, modulesDir);

    Runtime rt;
    rt.setLanguageRegistry(&registry);
    rt.loadProgram({
        makeCallFunc(kStubNamespaceId, kStubFnNoOp, 0),
        {OpCode::Halt, Value{}},
    });
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<std::string>(rt.stack()[0]) == "stub-ok");
}

TEST_CASE("LanguageModuleLoader missing directory skips and freezes registry") {
    LanguageRegistry registry;
    const auto missing = uniqueTempDir() / "no-such-modules";
    std::ostringstream log;
    const LanguageModuleLoadReport report = loadLanguageModules(registry, missing, &log);
    CHECK(report.attempted == 0);
    CHECK(report.loaded == 0);
    CHECK(report.failed == 0);
    CHECK(registry.isFrozen());
    CHECK(log.str().find("MODULES: (skip") != std::string::npos);
}

TEST_CASE("LanguageModuleLoader bad shared library is non-fatal") {
    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    {
        std::ofstream bad(modulesDir / "broken.so");
        bad << "not a shared library";
    }

    LanguageRegistry registry;
    std::ostringstream log;
    const LanguageModuleLoadReport report = loadLanguageModules(registry, modulesDir, &log);
    CHECK(report.attempted == 1);
    CHECK(report.loaded == 0);
    CHECK(report.failed == 1);
    CHECK(registry.isFrozen());
    CHECK(log.str().find("MODULE broken.so:") != std::string::npos);
}

TEST_CASE("LanguageModuleLoader duplicate namespace keeps first module") {
    const std::filesystem::path stubPath = stubModulePath();
    if (stubPath.empty() || !std::filesystem::exists(stubPath)) {
        MESSAGE("GEMINI_STUB_MODULE_PATH not configured; skipping");
        return;
    }

    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    const auto ext = stubPath.extension().string();
    std::filesystem::copy_file(stubPath, modulesDir / ("a" + ext));
    std::filesystem::copy_file(stubPath, modulesDir / ("b" + ext));

    LanguageRegistry registry;
    std::ostringstream log;
    const LanguageModuleLoadReport report = loadLanguageModules(registry, modulesDir, &log);
    CHECK(report.attempted == 2);
    CHECK(report.loaded == 1);
    CHECK(report.failed == 1);
    CHECK(registry.metadata(kStubNamespaceId).has_value());
    CHECK(log.str().find(std::string("MODULE a") + ext + ": OK") != std::string::npos);
    CHECK(log.str().find(std::string("MODULE b") + ext + ":") != std::string::npos);
}

TEST_CASE("LanguageModuleLoader logs boot-style lines") {
    const std::filesystem::path stubPath = stubModulePath();
    if (stubPath.empty() || !std::filesystem::exists(stubPath)) {
        MESSAGE("GEMINI_STUB_MODULE_PATH not configured; skipping");
        return;
    }

    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    std::filesystem::copy_file(stubPath, modulesDir / stubPath.filename());

    LanguageRegistry registry;
    std::ostringstream log;
    loadLanguageModules(registry, modulesDir, &log);
    const std::string output = log.str();
    CHECK(output.find("LOADING LANGUAGE MODULES") != std::string::npos);
    CHECK(output.find("MODULE ") != std::string::npos);
    CHECK(output.find(": OK") != std::string::npos);
}

TEST_CASE("LanguageModuleLoader loads built basic module") {
    const std::filesystem::path basicPath = basicModulePath();
    if (basicPath.empty() || !std::filesystem::exists(basicPath)) {
        MESSAGE("GEMINI_BASIC_MODULE_PATH not configured; skipping");
        return;
    }

    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    std::filesystem::copy_file(basicPath, modulesDir / basicPath.filename());

    LanguageRegistry registry;
    const LanguageModuleLoadReport report = loadLanguageModules(registry, modulesDir);
    CHECK(report.attempted == 1);
    CHECK(report.loaded == 1);
    CHECK(report.failed == 0);
    CHECK(registry.isFrozen());

    const auto meta = registry.metadata(kBasicNamespaceId);
    REQUIRE(meta.has_value());
    CHECK(meta->name == "basic");
    CHECK(meta->version == "1");
}

TEST_CASE("LanguageModuleLoader dispatches basic LEN and ABS after load") {
    const std::filesystem::path basicPath = basicModulePath();
    if (basicPath.empty() || !std::filesystem::exists(basicPath)) {
        MESSAGE("GEMINI_BASIC_MODULE_PATH not configured; skipping");
        return;
    }

    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    std::filesystem::copy_file(basicPath, modulesDir / basicPath.filename());

    LanguageRegistry registry;
    loadLanguageModules(registry, modulesDir);

    std::vector<Value> lenStack{std::string{"abc"}};
    registry.dispatch(kBasicNamespaceId, PickCore::Languages::Basic::kFnLen, lenStack, 1);
    REQUIRE(lenStack.size() == 1);
    CHECK(std::get<int>(lenStack.back()) == 3);

    std::vector<Value> absStack{-7};
    registry.dispatch(kBasicNamespaceId, PickCore::Languages::Basic::kFnAbs, absStack, 1);
    REQUIRE(absStack.size() == 1);
    CHECK(std::get<int>(absStack.back()) == 7);
}

TEST_CASE("LanguageModuleLoader basic CALL_FUNC end-to-end with loaded registry") {
    const std::filesystem::path basicPath = basicModulePath();
    if (basicPath.empty() || !std::filesystem::exists(basicPath)) {
        MESSAGE("GEMINI_BASIC_MODULE_PATH not configured; skipping");
        return;
    }

    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    std::filesystem::copy_file(basicPath, modulesDir / basicPath.filename());

    LanguageRegistry registry;
    loadLanguageModules(registry, modulesDir);

    Runtime rt;
    rt.setLanguageRegistry(&registry);
    rt.loadProgram({
        {OpCode::PushStr, std::string{"hello"}},
        makeCallFunc(kBasicNamespaceId, PickCore::Languages::Basic::kFnLen, 1),
        {OpCode::Halt, Value{}},
    });
    rt.run();
    REQUIRE(rt.stack().size() == 1);
    CHECK(std::get<int>(rt.stack()[0]) == 5);
}

TEST_CASE("LanguageModuleLoader loads pascal comal cobol stub modules") {
    const std::filesystem::path pascalPath = pascalModulePath();
    const std::filesystem::path comalPath = comalModulePath();
    const std::filesystem::path cobolPath = cobolModulePath();
    if (pascalPath.empty() || comalPath.empty() || cobolPath.empty() ||
        !std::filesystem::exists(pascalPath) || !std::filesystem::exists(comalPath) ||
        !std::filesystem::exists(cobolPath)) {
        MESSAGE("GEMINI_* stub module paths not configured; skipping");
        return;
    }

    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    copyModuleIfPresent(pascalPath, modulesDir);
    copyModuleIfPresent(comalPath, modulesDir);
    copyModuleIfPresent(cobolPath, modulesDir);

    LanguageRegistry registry;
    const LanguageModuleLoadReport report = loadLanguageModules(registry, modulesDir);
    CHECK(report.attempted == 3);
    CHECK(report.loaded == 3);
    CHECK(report.failed == 0);

    const auto pascalMeta = registry.metadata(Gemini::kNamespaceIdPascal);
    REQUIRE(pascalMeta.has_value());
    CHECK(pascalMeta->name == "pascal");
    CHECK(pascalMeta->version == "1");

    const auto comalMeta = registry.metadata(Gemini::kNamespaceIdComal);
    REQUIRE(comalMeta.has_value());
    CHECK(comalMeta->name == "comal");

    const auto cobolMeta = registry.metadata(Gemini::kNamespaceIdCobol);
    REQUIRE(cobolMeta.has_value());
    CHECK(cobolMeta->name == "cobol");
}

TEST_CASE("LanguageModuleLoader loads basic and language stubs together") {
    const std::filesystem::path basicPath = basicModulePath();
    const std::filesystem::path pascalPath = pascalModulePath();
    if (basicPath.empty() || pascalPath.empty() || !std::filesystem::exists(basicPath) ||
        !std::filesystem::exists(pascalPath)) {
        MESSAGE("GEMINI_BASIC_MODULE_PATH or GEMINI_PASCAL_MODULE_PATH not configured; skipping");
        return;
    }

    const auto modulesDir = uniqueTempDir() / "modules";
    std::filesystem::create_directories(modulesDir);
    copyModuleIfPresent(basicPath, modulesDir);
    copyModuleIfPresent(pascalPath, modulesDir);

    LanguageRegistry registry;
    const LanguageModuleLoadReport report = loadLanguageModules(registry, modulesDir);
    CHECK(report.loaded == 2);
    CHECK(registry.metadata(kBasicNamespaceId).has_value());
    CHECK(registry.metadata(Gemini::kNamespaceIdPascal).has_value());
}
