#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "DaemonConfig.h"

namespace {
    class ScopedEnv {
    public:
        void set(const char *name, const std::string &value) {
            save(name);
            setenv(name, value.c_str(), 1);
        }

        void unset(const char *name) {
            save(name);
            unsetenv(name);
        }

        ~ScopedEnv() {
            for (const Saved &saved: saved_) {
                if (saved.value.has_value()) {
                    setenv(saved.name.c_str(), saved.value->c_str(), 1);
                } else {
                    unsetenv(saved.name.c_str());
                }
            }
        }

    private:
        struct Saved {
            std::string name;
            std::optional<std::string> value;
        };

        void save(const char *name) {
            for (const Saved &existing: saved_) {
                if (existing.name == name) {
                    return;
                }
            }
            Saved saved{};
            saved.name = name;
            if (const char *const value = std::getenv(name)) {
                saved.value = value;
            }
            saved_.push_back(std::move(saved));
        }

        std::vector<Saved> saved_;
    };

    char *makeArgv(const std::vector<std::string> &args, std::vector<char *> &storage) {
        storage.clear();
        storage.reserve(args.size());
        for (const std::string &arg: args) {
            storage.push_back(const_cast<char *>(arg.c_str()));
        }
        return storage.empty() ? nullptr : storage[0];
    }
} // namespace

TEST_CASE("DaemonConfig embedded uses single session capacity") {
    const PickCore::DaemonConfig config = PickCore::DaemonConfig::embedded();
    CHECK(config.maxSessions == 1);
}

TEST_CASE("defaultDaemonSocketPath prefers XDG_RUNTIME_DIR") {
    ScopedEnv env;
    env.set("XDG_RUNTIME_DIR", "/tmp/pick-xdg-runtime");
    CHECK(PickCore::defaultDaemonSocketPath() == std::filesystem::path("/tmp/pick-xdg-runtime/gemini.sock"));
}

TEST_CASE("defaultDaemonSocketPath falls back to tmp") {
    ScopedEnv env;
    env.unset("XDG_RUNTIME_DIR");
    CHECK(PickCore::defaultDaemonSocketPath() == std::filesystem::path("/tmp/gemini.sock"));
}

TEST_CASE("resolveDaemonConfig applies env defaults") {
    ScopedEnv env;
    env.set("GEMINI_DAEMON_SOCKET", "/tmp/pick-env.sock");
    env.set("GEMINI_MAX_SESSIONS", "8");

    char arg0[] = "gemini-daemon";
    char *argv[] = {arg0};
    const PickCore::DaemonConfigResolution resolution = PickCore::resolveDaemonConfig(1, argv);
    CHECK_FALSE(resolution.showHelp);
    CHECK(resolution.config.socketPath == std::filesystem::path("/tmp/pick-env.sock"));
    CHECK(resolution.config.maxSessions == 8);
}

TEST_CASE("resolveDaemonConfig CLI overrides env") {
    ScopedEnv env;
    env.set("GEMINI_DAEMON_SOCKET", "/tmp/pick-env.sock");
    env.set("GEMINI_MAX_SESSIONS", "8");

    const std::vector<std::string> args = {
        "gemini-daemon",
        "--socket",
        "/tmp/pick-cli.sock",
        "--max-sessions",
        "16",
    };
    std::vector<char *> argvStorage;
    makeArgv(args, argvStorage);

    const PickCore::DaemonConfigResolution resolution =
        PickCore::resolveDaemonConfig(static_cast<int>(argvStorage.size()), argvStorage.data());
    CHECK(resolution.config.socketPath == std::filesystem::path("/tmp/pick-cli.sock"));
    CHECK(resolution.config.maxSessions == 16);
}

TEST_CASE("resolveDaemonConfig host path CLI overrides env") {
    ScopedEnv env;
    env.set("GEMINI_FILESYSTEM_ROOT", "/tmp/pick-env-account");
    env.set("GEMINI_CATALOG_ROOT", "/tmp/pick-env-catalog");

    const std::vector<std::string> args = {
        "gemini-daemon",
        "--pick-root",
        "/tmp/pick-cli-account",
        "--catalog-root",
        "/tmp/pick-cli-catalog",
        "--modules-root",
        "/tmp/pick-cli-modules",
    };
    std::vector<char *> argvStorage;
    makeArgv(args, argvStorage);

    const PickCore::DaemonConfigResolution resolution =
        PickCore::resolveDaemonConfig(static_cast<int>(argvStorage.size()), argvStorage.data());
    REQUIRE(resolution.config.hostPaths.pickFilesystemRoot.has_value());
    REQUIRE(resolution.config.hostPaths.geminiCatalogRoot.has_value());
    REQUIRE(resolution.config.hostPaths.geminiModulesRoot.has_value());
    CHECK(*resolution.config.hostPaths.pickFilesystemRoot == std::filesystem::path("/tmp/pick-cli-account"));
    CHECK(*resolution.config.hostPaths.geminiCatalogRoot == std::filesystem::path("/tmp/pick-cli-catalog"));
    CHECK(*resolution.config.hostPaths.geminiModulesRoot == std::filesystem::path("/tmp/pick-cli-modules"));
}

TEST_CASE("resolveDaemonConfig help flag") {
    char arg0[] = "gemini-daemon";
    char arg1[] = "--help";
    char *argv[] = {arg0, arg1};
    const PickCore::DaemonConfigResolution resolution = PickCore::resolveDaemonConfig(2, argv);
    CHECK(resolution.showHelp);
}
