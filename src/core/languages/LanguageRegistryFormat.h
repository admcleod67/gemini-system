#ifndef PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_REGISTRY_FORMAT_H
#define PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_REGISTRY_FORMAT_H

#include "LanguageModuleBootLog.h"
#include "LanguageRegistry.h"

#include <iosfwd>

namespace PickCore::Languages {
    void formatLanguagesReport(std::ostream &out,
                               const LanguageRegistry &registry,
                               const LanguageModuleBootLog &bootLog,
                               bool verbose = false);
} // namespace PickCore::Languages

#endif // PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_REGISTRY_FORMAT_H
