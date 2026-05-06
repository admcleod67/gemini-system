#ifndef PICK_SYSTEM_CORE_ENGLISH_DICTIONARY_RESOLVER_H
#define PICK_SYSTEM_CORE_ENGLISH_DICTIONARY_RESOLVER_H

#include "EnglishTypes.h"

#include "FileSystem.h"

#include <vector>

namespace PickCore::English {
    class DictionaryResolver {
    public:
        [[nodiscard]] std::vector<FieldRef> resolveFields(PickFS::FileSystem &fs,
                                                          const std::string &fileName,
                                                          const std::vector<std::string> &fields) const;
    };
} // namespace PickCore::English

#endif
