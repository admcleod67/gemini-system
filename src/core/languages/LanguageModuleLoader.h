#ifndef PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_MODULE_LOADER_H
#define PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_MODULE_LOADER_H

#include "LanguageRegistry.h"

#include <cstddef>
#include <filesystem>
#include <iosfwd>

namespace PickCore::Languages {
    struct LanguageModuleLoadReport {
        std::size_t attempted{0};
        std::size_t loaded{0};
        std::size_t failed{0};
    };

    LanguageModuleLoadReport loadLanguageModules(LanguageRegistry &registry,
                                                 const std::filesystem::path &modulesDir,
                                                 std::ostream *log = nullptr,
                                                 void *hostContext = nullptr);
} // namespace PickCore::Languages

#endif // PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_MODULE_LOADER_H
