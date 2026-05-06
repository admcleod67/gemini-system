#ifndef PICK_SYSTEM_CORE_ENGLISH_EXECUTOR_H
#define PICK_SYSTEM_CORE_ENGLISH_EXECUTOR_H

#include "DictionaryResolver.h"
#include "EnglishTypes.h"

#include "FileSystem.h"

namespace PickCore::English {
    class Executor {
    public:
        [[nodiscard]] Result execute(PickFS::FileSystem &fs,
                                     const Plan &plan,
                                     const DictionaryResolver &dictResolver,
                                     const EnglishRunOptions &opts,
                                     std::string &error) const;
    };
} // namespace PickCore::English

#endif
