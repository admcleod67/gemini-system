#include <doctest/doctest.h>

#include "BasicLanguageRegistration.h"
#include "LanguageModuleBootLog.h"
#include "LanguageRegistryFormat.h"
#include "TestLanguageNamespace.h"

#include <sstream>
#include <string>

using PickCore::Languages::LanguageModuleBootLog;
using PickCore::Languages::LanguageRegistry;
using PickCore::Languages::formatLanguagesReport;
using PickSystemTest::kTestNamespaceId;
using PickSystemTest::registerTestNamespace;

TEST_CASE("formatLanguagesReport lists namespaces and boot failures") {
    LanguageRegistry registry;
    registerTestNamespace(registry);

    LanguageModuleBootLog &bootLog = LanguageModuleBootLog::instance();
    bootLog.clear();
    bootLog.append("good.so", true);
    bootLog.append("bad.so", false, "not a shared library");

    std::ostringstream out;
    formatLanguagesReport(out, registry, bootLog, false);
    const std::string report = out.str();
    CHECK(report.find("Language namespaces (1):") != std::string::npos);
    CHECK(report.find("  " + std::to_string(kTestNamespaceId) + " test 1 3") != std::string::npos);
    CHECK(report.find("Module boot: 1 loaded, 1 failed (2 attempted)") != std::string::npos);
    CHECK(report.find("Boot failures:") != std::string::npos);
    CHECK(report.find("bad.so: not a shared library") != std::string::npos);
}

TEST_CASE("formatLanguagesReport verbose includes function slot arities") {
    LanguageRegistry registry;
    PickCore::Languages::Basic::registerBasicLanguage(registry);

    LanguageModuleBootLog &bootLog = LanguageModuleBootLog::instance();
    bootLog.clear();
    bootLog.append("basic.so", true);

    std::ostringstream out;
    formatLanguagesReport(out, registry, bootLog, true);
    const std::string report = out.str();
    CHECK(report.find("basic 1 28") != std::string::npos);
    CHECK(report.find("    0 1\n") != std::string::npos);
    CHECK(report.find("    25 0\n") != std::string::npos);
}
