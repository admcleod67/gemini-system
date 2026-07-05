#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <sstream>
#include <thread>

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
