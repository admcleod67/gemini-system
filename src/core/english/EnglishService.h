#ifndef PICK_SYSTEM_CORE_ENGLISH_SERVICE_H
#define PICK_SYSTEM_CORE_ENGLISH_SERVICE_H

#include "DictionaryResolver.h"
#include "EnglishParser.h"
#include "Executor.h"
#include "Planner.h"

namespace PickCore::English {
    class EnglishService {
    public:
        [[nodiscard]] std::optional<Result> run(PickFS::FileSystem &fs,
                                                const std::vector<std::string> &tokens,
                                                const ParseContext &parseCtx,
                                                const EnglishRunOptions &execOpts,
                                                std::string &error) const;

        [[nodiscard]] const DictionaryResolver &dictionaryResolver() const { return resolver_; }

    private:
        EnglishParser parser_;
        DictionaryResolver resolver_;
        Planner planner_;
        Executor executor_;
    };
} // namespace PickCore::English

#endif
