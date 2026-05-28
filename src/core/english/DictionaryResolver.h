#ifndef PICK_SYSTEM_CORE_ENGLISH_DICTIONARY_RESOLVER_H
#define PICK_SYSTEM_CORE_ENGLISH_DICTIONARY_RESOLVER_H

#include "EnglishTypes.h"

#include "FileSystem.h"

#include <string>
#include <vector>

namespace PickCore::English {
    /// Resolves projection and sort-key tokens via DICT. File-scoped dictionary logical name:
    /// `DICT-<dataFile>` when that file exists; then global `DICT`.
    class DictionaryResolver {
    public:
        [[nodiscard]] std::vector<FieldRef> resolveFields(PickFS::FileSystem &fs,
                                                        const std::string &dataFileName,
                                                        const std::vector<std::string> &fields) const;

        /// @param dataFileName Logical data file from the ENGLISH query; empty skips file-scoped DICT.
        [[nodiscard]] FieldRef resolveField(PickFS::FileSystem &fs,
                                            const std::string &dataFileName,
                                            const std::string &token) const;

        [[nodiscard]] static std::string scopedDictLogicalName(const std::string &dataFileName);

        [[nodiscard]] static std::string describeConversion(const FieldRef &ref);

        [[nodiscard]] static std::string describeFieldKind(const FieldRef &ref);

        [[nodiscard]] static std::string describeFSelector(const FCorrelativeDef &def);
    };
} // namespace PickCore::English

#endif
