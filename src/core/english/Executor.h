#ifndef PICK_SYSTEM_CORE_ENGLISH_EXECUTOR_H
#define PICK_SYSTEM_CORE_ENGLISH_EXECUTOR_H

#include "DictionaryResolver.h"
#include "EnglishTypes.h"
#include "Formatter.h"

#include "FileSystem.h"

#include <string>
#include <vector>

namespace PickCore::English {
    class Executor {
    public:
        [[nodiscard]] Result execute(PickFS::FileSystem &fs,
                                     const Plan &plan,
                                     const DictionaryResolver &dictResolver,
                                     const EnglishRunOptions &opts,
                                     std::string &error) const;

    private:
        /// Build the row stream + selectedIds + any verb-specific trailing summary lines
        /// (e.g. "5 records selected" for SELECT, the count for COUNT). On error, sets
        /// `error` and leaves the out-params in an unspecified state.
        void produceRows(PickFS::FileSystem &fs,
                         const Plan &plan,
                         const DictionaryResolver &dictResolver,
                         const EnglishRunOptions &opts,
                         std::vector<Row> &rows,
                         std::vector<std::string> &trailingLines,
                         std::vector<std::string> &selectedIds,
                         std::string &error) const;
    };
} // namespace PickCore::English

#endif
