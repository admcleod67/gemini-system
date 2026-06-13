#include "BasicLanguageRegistration.h"
#include "LanguageModuleAbi.h"

GEMINI_LANGUAGE_MODULE_EXPORT void register_language(PickCore::Languages::LanguageRegistry &registry) {
    PickCore::Languages::Basic::registerBasicLanguage(registry);
}
