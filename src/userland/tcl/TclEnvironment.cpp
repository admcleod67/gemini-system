#include "TclEnvironment.h"

#include <algorithm>
#include <utility>

namespace PickShell {
    bool TclEnvironment::set(std::string name, std::string value) {
        if (name.empty()) {
            return false;
        }
        variables_[std::move(name)] = std::move(value);
        return true;
    }

    std::optional<std::string> TclEnvironment::get(const std::string &name) const {
        const auto it = variables_.find(name);
        if (it == variables_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool TclEnvironment::unset(const std::string &name) {
        return variables_.erase(name) != 0U;
    }

    std::vector<std::string> TclEnvironment::names() const {
        std::vector<std::string> out;
        out.reserve(variables_.size());
        for (const auto &p: variables_) {
            out.push_back(p.first);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    void TclEnvironment::clear() {
        variables_.clear();
    }
} // namespace PickShell
