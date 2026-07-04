#include <doctest/doctest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "GeminiServiceDaemon.h"
#include "LanguageRegistry.h"
#include "LockRegistry.h"

static std::filesystem::path uniqueTempDir() {
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("pick-daemon-test-" + std::to_string(tick));
}

namespace {
    class ScopedHostPathEnv {
    public:
        ScopedHostPathEnv() {
            if (const char *const v = std::getenv("GEMINI_FILESYSTEM_ROOT")) {
                priorFilesystemRoot_ = v;
            }
            if (const char *const v = std::getenv("GEMINI_CATALOG_ROOT")) {
                priorCatalogRoot_ = v;
            }
        }

        void set(const std::filesystem::path &pickRoot, const std::filesystem::path &catalogRoot) {
            setenv("GEMINI_FILESYSTEM_ROOT", pickRoot.string().c_str(), 1);
            setenv("GEMINI_CATALOG_ROOT", catalogRoot.string().c_str(), 1);
        }

        ~ScopedHostPathEnv() {
            restore("GEMINI_FILESYSTEM_ROOT", priorFilesystemRoot_);
            restore("GEMINI_CATALOG_ROOT", priorCatalogRoot_);
        }

    private:
        static void restore(const char *name, const std::optional<std::string> &prior) {
            if (prior.has_value()) {
                setenv(name, prior->c_str(), 1);
            } else {
                unsetenv(name);
            }
        }

        std::optional<std::string> priorFilesystemRoot_;
        std::optional<std::string> priorCatalogRoot_;
    };
} // namespace

TEST_CASE("GeminiServiceDaemon coldStart prints boot milestones") {
    const auto root = uniqueTempDir();
    const auto gem = root / "gemini";
    const auto pick = gem / "accounts" / "SY";
    std::filesystem::create_directories(pick / "MD");
    std::filesystem::create_directories(pick / "VOC");
    {
        std::ofstream acc(gem / "ACCOUNTS.json");
        acc << R"({"accounts":[{"name":"SY","root":"accounts/SY"}]})";
    }

    ScopedHostPathEnv env;
    env.set(pick, gem);

    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::createEmbedded();
    std::ostringstream out;
    daemon.coldStart(out);
    const std::string s = out.str();
    CHECK(s.find("INITIALIZING SYSTEM") != std::string::npos);
    CHECK(s.find("MEMORY:") != std::string::npos);
    CHECK(s.find("MD ATTACHED") != std::string::npos);
    CHECK(s.find("VOC ATTACHED") != std::string::npos);
    CHECK(s.find("CATALOG ATTACHED") != std::string::npos);
    CHECK(s.find("PORT MANAGER:") != std::string::npos);
    CHECK(s.find("SYSTEM READY\n\n") != std::string::npos);
}

TEST_CASE("GeminiServiceDaemon coldStart freezes language registry") {
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::createEmbedded();
    std::ostringstream out;
    daemon.coldStart(out);
    CHECK(daemon.languageRegistry().isFrozen());
}

TEST_CASE("GeminiServiceDaemon coldStart is idempotent") {
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::createEmbedded();
    std::ostringstream out;
    daemon.coldStart(out);
    CHECK(daemon.isColdStarted());
    const std::string first = out.str();
    daemon.coldStart(out);
    CHECK(daemon.isColdStarted());
    CHECK(out.str() == first);
}

TEST_CASE("GeminiServiceDaemon lockTable delegates to LockRegistry") {
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::createEmbedded();
    const std::shared_ptr<PickCore::Locking::LockTable> table = daemon.lockTable();
    REQUIRE(table != nullptr);
    CHECK(table == PickCore::Locking::LockRegistry::instance().table());
}
