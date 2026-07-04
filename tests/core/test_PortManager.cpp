#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "PortManager.h"

TEST_CASE("PortManager allocate returns unique ports starting at 1") {
    PickCore::PortManager pm(4);
    const auto a = pm.allocate();
    const auto b = pm.allocate();
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(*a == 1);
    CHECK(*b == 2);
    CHECK(pm.inUseCount() == 2);
}

TEST_CASE("PortManager release reuses lowest free port") {
    PickCore::PortManager pm(4);
    const auto a = pm.allocate();
    const auto b = pm.allocate();
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    pm.release(*a);
    const auto c = pm.allocate();
    REQUIRE(c.has_value());
    CHECK(*c == 1);
    CHECK(pm.inUseCount() == 2);
}

TEST_CASE("PortManager returns nullopt at capacity") {
    PickCore::PortManager pm(2);
    REQUIRE(pm.allocate().has_value());
    REQUIRE(pm.allocate().has_value());
    CHECK_FALSE(pm.allocate().has_value());
}

TEST_CASE("PortManager release is idempotent for unknown port") {
    PickCore::PortManager pm(2);
    pm.release(99);
    const auto a = pm.allocate();
    REQUIRE(a.has_value());
    CHECK(*a == 1);
}

TEST_CASE("PortManager formatBootStatus prints READY") {
    PickCore::PortManager pm;
    std::ostringstream out;
    pm.formatBootStatus(out);
    CHECK(out.str() == "PORT MANAGER: READY\n");
}

TEST_CASE("PortManager concurrent allocate stays unique") {
    PickCore::PortManager pm(32);
    std::vector<PickCore::SessionId> ports;
    ports.reserve(16);
    std::mutex portsMutex;

    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < 2; ++j) {
                const auto port = pm.allocate();
                REQUIRE(port.has_value());
                std::lock_guard lock(portsMutex);
                ports.push_back(*port);
            }
        });
    }
    for (auto &t: threads) {
        t.join();
    }

    CHECK(ports.size() == 16);
    std::sort(ports.begin(), ports.end());
    const auto last = std::unique(ports.begin(), ports.end());
    CHECK(last == ports.end());
}
