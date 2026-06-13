#include "LanguageModuleBootLog.h"

namespace PickCore::Languages {
    void LanguageModuleBootLog::clear() {
        entries_.clear();
        attemptedCount_ = 0;
        loadedCount_ = 0;
        failedCount_ = 0;
    }

    void LanguageModuleBootLog::append(const std::string &fileName, const bool loaded, const std::string &detail) {
        ++attemptedCount_;
        if (loaded) {
            ++loadedCount_;
        } else {
            ++failedCount_;
        }
        entries_.push_back(LanguageModuleBootEntry{fileName, loaded, detail});
    }
} // namespace PickCore::Languages
