#ifndef PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_MODULE_ABI_H
#define PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_MODULE_ABI_H

namespace PickCore::Languages {
    class LanguageRegistry;

    using RegisterLanguageFn = void (*)(LanguageRegistry &);

    constexpr const char kRegisterLanguageSymbol[] = "register_language";

#if defined(_WIN32)
    #define GEMINI_LANGUAGE_MODULE_EXPORT extern "C" __declspec(dllexport)
#else
    #define GEMINI_LANGUAGE_MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#endif
} // namespace PickCore::Languages

#endif // PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_MODULE_ABI_H
