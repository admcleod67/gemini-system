#ifndef PICK_SYSTEM_TESTS_CORE_BASIC_LANGUAGE_TEST_FIXTURE_H
#define PICK_SYSTEM_TESTS_CORE_BASIC_LANGUAGE_TEST_FIXTURE_H

#include "BasicLanguageRegistration.h"
#include "LanguageRegistry.h"
#include "Runtime.h"

struct BasicRuntimeFixture {
    PickCore::Languages::LanguageRegistry registry;
    PickVM::Runtime rt;

    BasicRuntimeFixture() {
        PickCore::Languages::Basic::registerBasicLanguage(registry);
        rt.setLanguageRegistry(&registry);
    }
};

inline void registerBasicLanguageIn(PickCore::Languages::LanguageRegistry &registry) {
    PickCore::Languages::Basic::registerBasicLanguage(registry);
}

#endif // PICK_SYSTEM_TESTS_CORE_BASIC_LANGUAGE_TEST_FIXTURE_H
