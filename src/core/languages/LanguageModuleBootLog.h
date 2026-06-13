#ifndef PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_MODULE_BOOT_LOG_H
#define PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_MODULE_BOOT_LOG_H

#include <cstddef>
#include <string>
#include <vector>

namespace PickCore::Languages {
    struct LanguageModuleBootEntry {
        std::string fileName;
        bool loaded{false};
        std::string detail;
    };

    class LanguageModuleBootLog {
    public:
        [[nodiscard]] static LanguageModuleBootLog &instance() {
            static LanguageModuleBootLog log;
            return log;
        }

        void clear();
        void append(const std::string &fileName, const bool loaded, const std::string &detail = {});

        [[nodiscard]] const std::vector<LanguageModuleBootEntry> &entries() const { return entries_; }
        [[nodiscard]] std::size_t attemptedCount() const { return attemptedCount_; }
        [[nodiscard]] std::size_t loadedCount() const { return loadedCount_; }
        [[nodiscard]] std::size_t failedCount() const { return failedCount_; }

    private:
        LanguageModuleBootLog() = default;

        std::vector<LanguageModuleBootEntry> entries_;
        std::size_t attemptedCount_{0};
        std::size_t loadedCount_{0};
        std::size_t failedCount_{0};
    };
} // namespace PickCore::Languages

#endif // PICK_SYSTEM_CORE_LANGUAGES_LANGUAGE_MODULE_BOOT_LOG_H
