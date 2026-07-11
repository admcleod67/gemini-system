#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
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

    std::filesystem::path writeTempConfig(const std::string &name, const std::string &contents) {
        const auto path = std::filesystem::temp_directory_path() / name;
        std::ofstream out(path);
        REQUIRE(out);
        out << contents;
        return path;
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
    env.unset("GEMINI_DAEMON_CONFIG");
    env.set("GEMINI_DAEMON_SOCKET", "/tmp/pick-env.sock");
    env.set("GEMINI_MAX_SESSIONS", "8");

    char arg0[] = "gemini-daemon";
    char *argv[] = {arg0};
    const PickCore::DaemonConfigResolution resolution = PickCore::resolveDaemonConfig(1, argv);
    CHECK_FALSE(resolution.showHelp);
    CHECK_FALSE(resolution.error.has_value());
    CHECK(resolution.config.socketPath == std::filesystem::path("/tmp/pick-env.sock"));
    CHECK(resolution.config.maxSessions == 8);
}

TEST_CASE("resolveDaemonConfig CLI overrides env") {
    ScopedEnv env;
    env.unset("GEMINI_DAEMON_CONFIG");
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
    CHECK_FALSE(resolution.error.has_value());
    CHECK(resolution.config.socketPath == std::filesystem::path("/tmp/pick-cli.sock"));
    CHECK(resolution.config.maxSessions == 16);
}

TEST_CASE("resolveDaemonConfig host path CLI overrides env") {
    ScopedEnv env;
    env.unset("GEMINI_DAEMON_CONFIG");
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
    CHECK_FALSE(resolution.error.has_value());
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

TEST_CASE("loadDaemonConfigFile applies keys and ignores comments") {
    const auto path = writeTempConfig(
        "gemini-daemon-config-keys.conf",
        "# comment\n"
        "\n"
        "socket = /tmp/pick-file.sock\n"
        "max_sessions=12\n"
        "pick_root=/tmp/pick-file-account\n"
        "catalog_root=/tmp/pick-file-catalog\n"
        "modules_root=/tmp/pick-file-modules\n");

    PickCore::DaemonConfig config{};
    config.maxSessions = 64;
    const auto error = PickCore::loadDaemonConfigFile(path, config);
    CHECK_FALSE(error.has_value());
    CHECK(config.socketPath == std::filesystem::path("/tmp/pick-file.sock"));
    CHECK(config.maxSessions == 12);
    REQUIRE(config.hostPaths.pickFilesystemRoot.has_value());
    REQUIRE(config.hostPaths.geminiCatalogRoot.has_value());
    REQUIRE(config.hostPaths.geminiModulesRoot.has_value());
    CHECK(*config.hostPaths.pickFilesystemRoot == std::filesystem::path("/tmp/pick-file-account"));
    CHECK(*config.hostPaths.geminiCatalogRoot == std::filesystem::path("/tmp/pick-file-catalog"));
    CHECK(*config.hostPaths.geminiModulesRoot == std::filesystem::path("/tmp/pick-file-modules"));
}

TEST_CASE("resolveDaemonConfig loads file via --config") {
    ScopedEnv env;
    env.unset("GEMINI_DAEMON_CONFIG");
    env.unset("GEMINI_DAEMON_SOCKET");
    env.unset("GEMINI_MAX_SESSIONS");

    const auto path = writeTempConfig("gemini-daemon-config-cli.conf",
                                      "socket=/tmp/pick-config-cli.sock\nmax_sessions=7\n");
    const std::vector<std::string> args = {"gemini-daemon", "--config", path.string()};
    std::vector<char *> argvStorage;
    makeArgv(args, argvStorage);

    const PickCore::DaemonConfigResolution resolution =
        PickCore::resolveDaemonConfig(static_cast<int>(argvStorage.size()), argvStorage.data());
    CHECK_FALSE(resolution.error.has_value());
    CHECK(resolution.config.socketPath == std::filesystem::path("/tmp/pick-config-cli.sock"));
    CHECK(resolution.config.maxSessions == 7);
}

TEST_CASE("resolveDaemonConfig loads file via GEMINI_DAEMON_CONFIG") {
    ScopedEnv env;
    env.unset("GEMINI_DAEMON_SOCKET");
    env.unset("GEMINI_MAX_SESSIONS");

    const auto path = writeTempConfig("gemini-daemon-config-env.conf",
                                      "socket=/tmp/pick-config-env.sock\nmax_sessions=9\n");
    env.set("GEMINI_DAEMON_CONFIG", path.string());

    char arg0[] = "gemini-daemon";
    char *argv[] = {arg0};
    const PickCore::DaemonConfigResolution resolution = PickCore::resolveDaemonConfig(1, argv);
    CHECK_FALSE(resolution.error.has_value());
    CHECK(resolution.config.socketPath == std::filesystem::path("/tmp/pick-config-env.sock"));
    CHECK(resolution.config.maxSessions == 9);
}

TEST_CASE("resolveDaemonConfig env overrides file and CLI overrides env") {
    ScopedEnv env;
    const auto path = writeTempConfig("gemini-daemon-config-precedence.conf",
                                      "socket=/tmp/pick-file.sock\nmax_sessions=3\n"
                                      "pick_root=/tmp/pick-file-account\n");
    env.set("GEMINI_DAEMON_CONFIG", path.string());
    env.set("GEMINI_DAEMON_SOCKET", "/tmp/pick-env.sock");
    env.set("GEMINI_MAX_SESSIONS", "5");
    env.set("GEMINI_FILESYSTEM_ROOT", "/tmp/pick-env-account");

    const std::vector<std::string> args = {
        "gemini-daemon",
        "--socket",
        "/tmp/pick-cli.sock",
        "--max-sessions",
        "11",
        "--pick-root",
        "/tmp/pick-cli-account",
    };
    std::vector<char *> argvStorage;
    makeArgv(args, argvStorage);

    const PickCore::DaemonConfigResolution resolution =
        PickCore::resolveDaemonConfig(static_cast<int>(argvStorage.size()), argvStorage.data());
    CHECK_FALSE(resolution.error.has_value());
    CHECK(resolution.config.socketPath == std::filesystem::path("/tmp/pick-cli.sock"));
    CHECK(resolution.config.maxSessions == 11);
    REQUIRE(resolution.config.hostPaths.pickFilesystemRoot.has_value());
    CHECK(*resolution.config.hostPaths.pickFilesystemRoot == std::filesystem::path("/tmp/pick-cli-account"));
}

TEST_CASE("resolveDaemonConfig --config overrides GEMINI_DAEMON_CONFIG path") {
    ScopedEnv env;
    env.unset("GEMINI_DAEMON_SOCKET");
    env.unset("GEMINI_MAX_SESSIONS");

    const auto envPath =
        writeTempConfig("gemini-daemon-config-path-env.conf", "socket=/tmp/pick-from-env.sock\n");
    const auto cliPath =
        writeTempConfig("gemini-daemon-config-path-cli.conf", "socket=/tmp/pick-from-cli.sock\n");
    env.set("GEMINI_DAEMON_CONFIG", envPath.string());

    const std::vector<std::string> args = {"gemini-daemon", "--config", cliPath.string()};
    std::vector<char *> argvStorage;
    makeArgv(args, argvStorage);

    const PickCore::DaemonConfigResolution resolution =
        PickCore::resolveDaemonConfig(static_cast<int>(argvStorage.size()), argvStorage.data());
    CHECK_FALSE(resolution.error.has_value());
    CHECK(resolution.config.socketPath == std::filesystem::path("/tmp/pick-from-cli.sock"));
}

TEST_CASE("loadDaemonConfigFile rejects unknown key") {
    const auto path = writeTempConfig("gemini-daemon-config-unknown.conf", "nope=1\n");
    PickCore::DaemonConfig config{};
    const auto error = PickCore::loadDaemonConfigFile(path, config);
    REQUIRE(error.has_value());
    CHECK(error->find("unknown config key") != std::string::npos);
}

TEST_CASE("loadDaemonConfigFile rejects malformed line") {
    const auto path = writeTempConfig("gemini-daemon-config-malformed.conf", "not-a-pair\n");
    PickCore::DaemonConfig config{};
    const auto error = PickCore::loadDaemonConfigFile(path, config);
    REQUIRE(error.has_value());
    CHECK(error->find("expected key=value") != std::string::npos);
}

TEST_CASE("loadDaemonConfigFile rejects invalid max_sessions") {
    const auto path = writeTempConfig("gemini-daemon-config-bad-max.conf", "max_sessions=0\n");
    PickCore::DaemonConfig config{};
    const auto error = PickCore::loadDaemonConfigFile(path, config);
    REQUIRE(error.has_value());
    CHECK(error->find("max_sessions") != std::string::npos);
}

TEST_CASE("resolveDaemonConfig missing config file is an error") {
    ScopedEnv env;
    env.unset("GEMINI_DAEMON_CONFIG");

    const std::vector<std::string> args = {"gemini-daemon", "--config",
                                           "/tmp/gemini-daemon-config-does-not-exist.conf"};
    std::vector<char *> argvStorage;
    makeArgv(args, argvStorage);

    const PickCore::DaemonConfigResolution resolution =
        PickCore::resolveDaemonConfig(static_cast<int>(argvStorage.size()), argvStorage.data());
    REQUIRE(resolution.error.has_value());
    CHECK(resolution.error->find("cannot open config file") != std::string::npos);
}

TEST_CASE("resolveDaemonConfig --config without path is an error") {
    ScopedEnv env;
    env.unset("GEMINI_DAEMON_CONFIG");

    const std::vector<std::string> args = {"gemini-daemon", "--config"};
    std::vector<char *> argvStorage;
    makeArgv(args, argvStorage);

    const PickCore::DaemonConfigResolution resolution =
        PickCore::resolveDaemonConfig(static_cast<int>(argvStorage.size()), argvStorage.data());
    REQUIRE(resolution.error.has_value());
    CHECK(resolution.error->find("--config requires a path") != std::string::npos);
}
