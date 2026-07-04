#include "TestProcessReset.h"

#include "LanguageModuleBootLog.h"
#include "LanguageRegistry.h"
#include "LockRegistry.h"

#include <doctest/doctest.h>

namespace {
    void resetProcessSingletonsForTests() {
        PickCore::Locking::LockRegistry::resetForTests();
        PickCore::Languages::LanguageRegistry::instance().resetForTests();
        PickCore::Languages::LanguageModuleBootLog::instance().clear();
    }

    struct TestProcessResetListener final : doctest::IReporter {
        explicit TestProcessResetListener(const doctest::ContextOptions &) {}

        void report_query(const doctest::QueryData &) override {}

        void test_run_start() override {}

        void test_run_end(const doctest::TestRunStats &) override {}

        void test_case_start(const doctest::TestCaseData &) override {}

        void test_case_reenter(const doctest::TestCaseData &) override {}

        void test_case_end(const doctest::CurrentTestCaseStats &) override { resetProcessSingletonsForTests(); }

        void test_case_exception(const doctest::TestCaseException &) override {}

        void subcase_start(const doctest::SubcaseSignature &) override {}

        void subcase_end() override {}

        void log_assert(const doctest::AssertData &) override {}

        void log_message(const doctest::MessageData &) override {}

        void test_case_skipped(const doctest::TestCaseData &) override {}
    };
} // namespace

REGISTER_LISTENER("test process reset", 0, TestProcessResetListener);
