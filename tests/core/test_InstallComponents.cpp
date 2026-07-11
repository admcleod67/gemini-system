#include <doctest/doctest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifndef PICK_SYSTEM_CMAKE_COMMAND
#define PICK_SYSTEM_CMAKE_COMMAND "cmake"
#endif

#ifndef PICK_SYSTEM_BINARY_DIR
#define PICK_SYSTEM_BINARY_DIR ""
#endif

#ifndef PICK_SYSTEM_INSTALL_BINDIR
#define PICK_SYSTEM_INSTALL_BINDIR "bin"
#endif

#ifndef PICK_SYSTEM_INSTALL_DATADIR
#define PICK_SYSTEM_INSTALL_DATADIR "share"
#endif

#ifndef PICK_SYSTEM_INSTALL_SYSCONFDIR
#define PICK_SYSTEM_INSTALL_SYSCONFDIR "etc"
#endif

#ifndef PICK_SYSTEM_INSTALL_LIBDIR
#define PICK_SYSTEM_INSTALL_LIBDIR "lib"
#endif

namespace {
    [[nodiscard]] std::filesystem::path uniqueTempDir() {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto root =
            std::filesystem::temp_directory_path() / ("pick-install-components-" + std::to_string(tick));
        std::filesystem::create_directories(root);
        return root;
    }

    [[nodiscard]] int runCmakeInstall(const std::filesystem::path &prefix, const std::string &component) {
        const std::string cmd = std::string(PICK_SYSTEM_CMAKE_COMMAND) + " --install \"" +
                                std::string(PICK_SYSTEM_BINARY_DIR) + "\" --prefix \"" + prefix.string() +
                                "\" --component " + component;
        return std::system(cmd.c_str());
    }

    [[nodiscard]] bool hasAnyModule(const std::filesystem::path &modulesDir) {
        if (!std::filesystem::is_directory(modulesDir)) {
            return false;
        }
        for (const auto &entry : std::filesystem::directory_iterator(modulesDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto name = entry.path().filename().string();
            if (name.find("gemini-module") != std::string::npos) {
                return true;
            }
        }
        return false;
    }
} // namespace

TEST_CASE("Application component install excludes service binaries") {
    REQUIRE(std::string(PICK_SYSTEM_BINARY_DIR).size() > 0);

    const std::filesystem::path prefix = uniqueTempDir();
    REQUIRE(runCmakeInstall(prefix, "Runtime") == 0);
    REQUIRE(runCmakeInstall(prefix, "Application") == 0);

    const auto bin = prefix / PICK_SYSTEM_INSTALL_BINDIR;
    const auto data = prefix / PICK_SYSTEM_INSTALL_DATADIR / "gemini";

    CHECK(std::filesystem::exists(bin / "gemini-system"));
    CHECK_FALSE(std::filesystem::exists(bin / "gemini-daemon"));
    CHECK_FALSE(std::filesystem::exists(bin / "gemini-console"));
    CHECK(std::filesystem::exists(data / "ACCOUNTS.json"));
    CHECK(hasAnyModule(data / "modules"));
    CHECK_FALSE(std::filesystem::exists(prefix / PICK_SYSTEM_INSTALL_SYSCONFDIR / "gemini" / "daemon.conf"));
}

TEST_CASE("Service component install excludes gemini-system") {
    REQUIRE(std::string(PICK_SYSTEM_BINARY_DIR).size() > 0);

    const std::filesystem::path prefix = uniqueTempDir();
    REQUIRE(runCmakeInstall(prefix, "Runtime") == 0);
    REQUIRE(runCmakeInstall(prefix, "Service") == 0);

    const auto bin = prefix / PICK_SYSTEM_INSTALL_BINDIR;
    const auto data = prefix / PICK_SYSTEM_INSTALL_DATADIR / "gemini";

    CHECK(std::filesystem::exists(bin / "gemini-daemon"));
    CHECK(std::filesystem::exists(bin / "gemini-console"));
    CHECK_FALSE(std::filesystem::exists(bin / "gemini-system"));
    CHECK(std::filesystem::exists(data / "ACCOUNTS.json"));
    CHECK(hasAnyModule(data / "modules"));
    CHECK(std::filesystem::exists(prefix / PICK_SYSTEM_INSTALL_SYSCONFDIR / "gemini" / "daemon.conf"));
    CHECK(std::filesystem::exists(prefix / PICK_SYSTEM_INSTALL_LIBDIR / "systemd" / "system" / "gemini.service"));
    CHECK(std::filesystem::exists(prefix / PICK_SYSTEM_INSTALL_DATADIR / "doc" / "gemini" / "daemon.conf.example"));
}
