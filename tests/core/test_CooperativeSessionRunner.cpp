#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>

#include "CooperativeSessionRunner.h"
#include "DaemonConfig.h"
#include "GeminiServiceDaemon.h"
#include "GeminiSessionHost.h"

TEST_CASE("CooperativeSessionRunner runExclusive excludes concurrent execution") {
    PickCore::CooperativeSessionRunner runner;
    std::atomic<bool> bStarted{false};
    std::atomic<bool> bBlockedDuringA{false};

    std::thread threadA([&] {
        runner.runExclusive(1, [&] {
            bStarted = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        });
    });

    std::thread threadB([&] {
        while (!bStarted.load()) {
            std::this_thread::yield();
        }
        std::atomic<bool> entered{false};
        std::thread waiter([&] {
            runner.runExclusive(2, [&] { entered = true; });
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        bBlockedDuringA = !entered.load();
        waiter.join();
    });

    threadA.join();
    threadB.join();
    CHECK(bBlockedDuringA.load());
}

TEST_CASE("CooperativeSessionRunner yield allows another session to acquire") {
    PickCore::CooperativeSessionRunner runner;
    std::atomic<bool> sessionTwoAcquired{false};
    std::atomic<bool> sessionOneResumed{false};

    std::thread sessionOne([&] {
        runner.acquire(1);
        REQUIRE(runner.activeSession() == 1);
        runner.yieldWaitingForInput(1);
        CHECK(runner.state(1) == PickCore::SessionRunState::WaitingForInput);
        CHECK(runner.activeSession() != std::optional<PickCore::SessionId>{1});

        while (!sessionOneResumed.load()) {
            std::this_thread::yield();
        }

        runner.acquire(1);
        CHECK(runner.activeSession() == 1);
        runner.release(1);
    });

    std::thread sessionTwo([&] {
        while (runner.state(1) != PickCore::SessionRunState::WaitingForInput) {
            std::this_thread::yield();
        }

        runner.acquire(2);
        sessionTwoAcquired = true;
        CHECK(runner.activeSession() == 2);
        runner.release(2);
    });

    while (!sessionTwoAcquired.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    runner.resume(1);
    sessionOneResumed = true;

    sessionOne.join();
    sessionTwo.join();

    CHECK(runner.state(1) == PickCore::SessionRunState::Runnable);
    CHECK(runner.state(2) == PickCore::SessionRunState::Runnable);
    CHECK_FALSE(runner.activeSession().has_value());
}

TEST_CASE("CooperativeSessionRunner resume on never-yielded session stays Runnable") {
    PickCore::CooperativeSessionRunner runner;
    runner.resume(1);
    CHECK(runner.state(1) == PickCore::SessionRunState::Runnable);
    CHECK_FALSE(runner.activeSession().has_value());
}

TEST_CASE("CooperativeSessionRunner release from non-holder throws") {
    PickCore::CooperativeSessionRunner runner;
    runner.acquire(1);
    CHECK_THROWS_AS(runner.release(2), std::logic_error);
    runner.release(1);
}

TEST_CASE("CooperativeSessionRunner yield from non-holder throws") {
    PickCore::CooperativeSessionRunner runner;
    CHECK_THROWS_AS(runner.yieldWaitingForInput(1), std::logic_error);
}

TEST_CASE("GeminiSessionHost cooperative acquire yield and resume") {
    PickCore::DaemonConfig config{};
    config.maxSessions = 2;
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::GeminiSessionHost host(daemon, 2);
    const PickShell::SessionHandle ha = host.createSession();
    const PickShell::SessionHandle hb = host.createSession();

    std::atomic<bool> sessionTwoAcquired{false};
    std::atomic<bool> sessionOneResumed{false};

    std::thread sessionOne([&] {
        host.acquire(ha.id);
        host.yieldWaitingForInput(ha.id);
        CHECK(host.sessionRunState(ha.id) == PickCore::SessionRunState::WaitingForInput);

        while (!sessionOneResumed.load()) {
            std::this_thread::yield();
        }

        host.acquire(ha.id);
        host.release(ha.id);
    });

    std::thread sessionTwo([&] {
        while (host.sessionRunState(ha.id) != PickCore::SessionRunState::WaitingForInput) {
            std::this_thread::yield();
        }

        host.acquire(hb.id);
        sessionTwoAcquired = true;
        host.release(hb.id);
    });

    while (!sessionTwoAcquired.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    host.resume(ha.id);
    sessionOneResumed = true;

    sessionOne.join();
    sessionTwo.join();

    CHECK(host.sessionRunState(ha.id) == PickCore::SessionRunState::Runnable);
    CHECK(host.sessionRunState(hb.id) == PickCore::SessionRunState::Runnable);
}

TEST_CASE("CooperativeSessionRunner round robin elects next runnable session by port order") {
    PickCore::CooperativeSessionRunner runner;
    runner.resume(1);
    runner.resume(2);
    runner.resume(3);

    runner.acquire(1);
    runner.release(1);
    CHECK(runner.nextRunnableSession() == 2);

    runner.acquire(2);
    runner.release(2);
    CHECK(runner.nextRunnableSession() == 3);

    runner.acquire(3);
    runner.release(3);
    CHECK(runner.nextRunnableSession() == 1);
}

TEST_CASE("CooperativeSessionRunner three session round robin alternates grants") {
    PickCore::CooperativeSessionRunner runner;
    runner.resume(1);
    runner.resume(2);
    runner.resume(3);

    std::mutex orderMutex;
    std::vector<PickCore::SessionId> grantOrder;
    std::atomic<int> cycleCount{0};
    constexpr int kCycles = 6;

    std::thread sessionOne([&] {
        for (int cycle = 0; cycle < kCycles; ++cycle) {
            runner.acquire(1);
            {
                const std::lock_guard lock(orderMutex);
                grantOrder.push_back(1);
            }
            runner.release(1);
            cycleCount.fetch_add(1, std::memory_order_release);
        }
    });

    std::thread sessionTwo([&] {
        for (int cycle = 0; cycle < kCycles; ++cycle) {
            runner.acquire(2);
            {
                const std::lock_guard lock(orderMutex);
                grantOrder.push_back(2);
            }
            runner.release(2);
            cycleCount.fetch_add(1, std::memory_order_release);
        }
    });

    std::thread sessionThree([&] {
        for (int cycle = 0; cycle < kCycles; ++cycle) {
            runner.acquire(3);
            {
                const std::lock_guard lock(orderMutex);
                grantOrder.push_back(3);
            }
            runner.release(3);
            cycleCount.fetch_add(1, std::memory_order_release);
        }
    });

    sessionOne.join();
    sessionTwo.join();
    sessionThree.join();

    CHECK(cycleCount.load() == kCycles * 3);
    REQUIRE(grantOrder.size() == static_cast<std::size_t>(kCycles * 3));

    for (std::size_t index = 1; index < grantOrder.size(); ++index) {
        const PickCore::SessionId previous = grantOrder[index - 1];
        const PickCore::SessionId current = grantOrder[index];
        if (previous == 1) {
            CHECK(current == 2);
        } else if (previous == 2) {
            CHECK(current == 3);
        } else {
            CHECK(current == 1);
        }
    }
}

TEST_CASE("CooperativeSessionRunner yield starvation regression gives other sessions turns") {
    PickCore::CooperativeSessionRunner runner;
    runner.resume(2);
    runner.resume(3);

    std::atomic<bool> sessionTwoAcquired{false};
    std::atomic<bool> sessionThreeAcquired{false};
    std::atomic<bool> sessionOneResumed{false};

    std::thread sessionOne([&] {
        runner.acquire(1);
        runner.yieldWaitingForInput(1);

        while (!sessionOneResumed.load()) {
            std::this_thread::yield();
        }

        runner.acquire(1);
        runner.release(1);
    });

    std::thread sessionTwo([&] {
        while (runner.state(1) != PickCore::SessionRunState::WaitingForInput) {
            std::this_thread::yield();
        }

        runner.acquire(2);
        sessionTwoAcquired = true;
        runner.release(2);
    });

    std::thread sessionThree([&] {
        while (!sessionTwoAcquired.load()) {
            std::this_thread::yield();
        }

        runner.acquire(3);
        sessionThreeAcquired = true;
        runner.release(3);
    });

    while (!sessionThreeAcquired.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    runner.resume(1);
    sessionOneResumed = true;

    sessionOne.join();
    sessionTwo.join();
    sessionThree.join();

    CHECK(sessionTwoAcquired.load());
    CHECK(sessionThreeAcquired.load());
    CHECK(runner.state(1) == PickCore::SessionRunState::Runnable);
    CHECK(runner.state(2) == PickCore::SessionRunState::Runnable);
    CHECK(runner.state(3) == PickCore::SessionRunState::Runnable);
}

TEST_CASE("CooperativeSessionRunner resume does not skip round robin turn") {
    PickCore::CooperativeSessionRunner runner;
    runner.acquire(1);
    runner.yieldWaitingForInput(1);
    runner.resume(2);
    runner.resume(3);

    CHECK(runner.nextRunnableSession() == 2);

    runner.acquire(2);
    runner.release(2);
    runner.resume(1);

    CHECK(runner.nextRunnableSession() == 3);
}

TEST_CASE("CooperativeSessionRunner retireSession removes zombie from round robin") {
    PickCore::CooperativeSessionRunner runner;
    runner.acquire(1);
    runner.release(1);
    runner.retireSession(1);
    runner.resume(2);

    runner.acquire(2);
    CHECK(runner.activeSession() == 2);
    runner.release(2);
}

TEST_CASE("CooperativeSessionRunner acquireAfterInputWake bypasses round robin for woken session") {
    PickCore::CooperativeSessionRunner runner;
    runner.acquire(1);
    runner.release(1);
    runner.retireSession(1);
    runner.resume(2);

    runner.acquireAfterInputWake(2);
    CHECK(runner.activeSession() == 2);
    runner.release(2);
}
