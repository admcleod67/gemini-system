#include "EnglishParser.h"

namespace PickCore::English {
    std::optional<Query> EnglishParser::parse(const std::vector<std::string> &tokens, std::string &error) const {
        if (tokens.empty()) {
            error = "Empty query";
            return std::nullopt;
        }
        if (tokens.size() < 2) {
            error = "ENGLISH command requires a filename";
            return std::nullopt;
        }

        Query q{};
        if (tokens[0] == "LIST") {
            q.verb = Verb::LIST;
        } else if (tokens[0] == "COUNT") {
            q.verb = Verb::COUNT;
        } else if (tokens[0] == "SELECT") {
            q.verb = Verb::SELECT;
        } else {
            error = "Unsupported ENGLISH verb";
            return std::nullopt;
        }
        q.fileName = tokens[1];
        for (std::size_t i = 2; i < tokens.size(); ++i) {
            q.fields.push_back(tokens[i]);
        }
        return q;
    }
} // namespace PickCore::English
