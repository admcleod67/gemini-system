#include "EnglishService.h"

namespace PickCore::English {
    std::optional<Result> EnglishService::run(PickFS::FileSystem &fs,
                                              const std::vector<std::string> &tokens,
                                              std::string &error) const {
        const std::optional<Query> query = parser_.parse(tokens, error);
        if (!query.has_value()) {
            return std::nullopt;
        }
        const Plan plan = planner_.makePlan(*query);
        Result result = executor_.execute(fs, plan, resolver_, error);
        if (!error.empty()) {
            return std::nullopt;
        }
        return result;
    }
} // namespace PickCore::English
