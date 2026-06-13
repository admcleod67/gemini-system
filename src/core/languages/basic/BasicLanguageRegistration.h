#ifndef PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_LANGUAGE_REGISTRATION_H
#define PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_LANGUAGE_REGISTRATION_H

namespace PickCore::Languages {
    class LanguageRegistry;

    namespace Basic {
        void registerBasicLanguage(LanguageRegistry &registry, void *hostContext = nullptr);
    } // namespace Basic
} // namespace PickCore::Languages

#endif // PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_LANGUAGE_REGISTRATION_H
