#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {
    std::string readFile(const std::filesystem::path &path) {
        std::ifstream in(path);
        REQUIRE(in);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
} // namespace

TEST_CASE("systemd gemini.service unit contains required Stage 3 keys") {
    const std::filesystem::path unitPath = PICK_SYSTEM_GEMINI_SERVICE_UNIT;
    REQUIRE(std::filesystem::exists(unitPath));
    const std::string text = readFile(unitPath);

    CHECK(text.find("Type=simple") != std::string::npos);
    CHECK(text.find("gemini-daemon --config") != std::string::npos);
    CHECK(text.find("StandardOutput=journal") != std::string::npos);
    CHECK(text.find("StandardError=journal") != std::string::npos);
    CHECK(text.find("RuntimeDirectory=gemini") != std::string::npos);
    CHECK(text.find("WantedBy=multi-user.target") != std::string::npos);
    CHECK(text.find("/gemini/daemon.conf") != std::string::npos);
}

TEST_CASE("packaging daemon.conf defaults to /run/gemini socket") {
    const std::filesystem::path confPath = PICK_SYSTEM_GEMINI_DAEMON_CONF;
    REQUIRE(std::filesystem::exists(confPath));
    const std::string text = readFile(confPath);

    CHECK(text.find("socket=/run/gemini/gemini.sock") != std::string::npos);
    CHECK(text.find("max_sessions=64") != std::string::npos);
}
