#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>
#include <vector>

#include "CooperativeSessionRunner.h"
#include "DaemonConfig.h"
#include "GeminiServiceDaemon.h"
#include "GeminiSessionHost.h"
#include "IpcSessionChannel.h"

namespace {
    void bindRunnerScheduling(PickCore::CooperativeSessionRunner &runner,
                              const PickCore::SessionId id,
                              PickCore::IpcSessionChannel &channel) {
        channel.setSessionScheduling(PickCore::SessionInputScheduling{
            [&runner, id] { runner.yieldWaitingForInput(id); },
            [&runner, id] { runner.acquire(id); },
            [&runner, id] { runner.resume(id); },
        });
    }
} // namespace

TEST_CASE("Schedulable IpcSessionChannel blocked read releases execution token") {
    PickCore::CooperativeSessionRunner runner;
    PickCore::IpcSessionChannel channel;
    bindRunnerScheduling(runner, 1, channel);

    std::atomic<bool> readStarted{false};
    std::atomic<bool> readComplete{false};
    std::atomic<int> readValue{-1};
    std::atomic<bool> sessionTwoAcquired{false};

    std::thread sessionOne([&] {
        readStarted = true;
        runner.acquire(1);
        readValue = channel.input().get();
        readComplete = true;
        runner.release(1);
    });

    std::thread sessionTwo([&] {
        while (!readStarted.load()) {
            std::this_thread::yield();
        }
        while (runner.state(1) != PickCore::SessionRunState::WaitingForInput) {
            std::this_thread::yield();
        }
        CHECK_FALSE(runner.activeSession().has_value());

        runner.acquire(2);
        sessionTwoAcquired = true;
        runner.release(2);
    });

    while (!sessionTwoAcquired.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    channel.pushInput(std::vector<std::uint8_t>{'x'});

    sessionOne.join();
    sessionTwo.join();

    CHECK(readComplete.load());
    CHECK(readValue.load() == 'x');
    CHECK(runner.state(1) == PickCore::SessionRunState::Runnable);
    CHECK(runner.state(2) == PickCore::SessionRunState::Runnable);
    CHECK_FALSE(runner.activeSession().has_value());
}

TEST_CASE("GeminiSessionHost bindIpcChannelScheduling blocked read alternates sessions") {
    PickCore::DaemonConfig config{};
    config.maxSessions = 2;
    PickCore::GeminiServiceDaemon daemon = PickCore::GeminiServiceDaemon::create(config);
    std::ostringstream boot;
    daemon.coldStart(boot);

    PickShell::GeminiSessionHost host(daemon, 2);
    const PickShell::SessionHandle ha = host.createSession();
    const PickShell::SessionHandle hb = host.createSession();

    PickCore::IpcSessionChannel channel;
    host.bindIpcChannelScheduling(ha.id, channel);

    std::atomic<bool> readComplete{false};
    std::atomic<int> readValue{-1};
    std::atomic<bool> sessionTwoAcquired{false};

    std::thread sessionOne([&] {
        host.acquire(ha.id);
        readValue = channel.input().get();
        readComplete = true;
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

    channel.pushInput(std::vector<std::uint8_t>{'y'});

    sessionOne.join();
    sessionTwo.join();

    CHECK(readComplete.load());
    CHECK(readValue.load() == 'y');
    CHECK(host.sessionRunState(ha.id) == PickCore::SessionRunState::Runnable);
    CHECK(host.sessionRunState(hb.id) == PickCore::SessionRunState::Runnable);
}

TEST_CASE("IpcSessionChannel without scheduling reads pushed input bytes") {
    PickCore::IpcSessionChannel channel;
    CHECK_FALSE(channel.hasSessionScheduling());

    const std::vector<std::uint8_t> bytes = {'h', 'i'};
    channel.pushInput(bytes);

    CHECK(channel.input().get() == 'h');
    CHECK(channel.input().get() == 'i');
}

TEST_CASE("IpcSessionChannel pushInput without scheduling does not resume scheduler") {
    PickCore::CooperativeSessionRunner runner;
    PickCore::IpcSessionChannel channel;
    CHECK_FALSE(channel.hasSessionScheduling());

    std::atomic<bool> readComplete{false};

    std::thread reader([&] {
        runner.acquire(1);
        CHECK(channel.input().get() == 'z');
        readComplete = true;
        runner.release(1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    channel.pushInput(std::vector<std::uint8_t>{'z'});

    reader.join();
    CHECK(readComplete.load());
    CHECK(runner.state(1) == PickCore::SessionRunState::Runnable);
}
