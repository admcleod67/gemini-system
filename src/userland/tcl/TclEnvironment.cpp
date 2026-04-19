#include "TclEnvironment.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace PickShell {
    std::string TclEnvironment::canonicalVariableName(const std::string_view name) {
        std::string out(name);
        for (char &c: out) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return out;
    }

    bool TclEnvironment::set(std::string name, std::string value) {
        if (name.empty()) {
            return false;
        }
        variables_[canonicalVariableName(name)] = std::move(value);
        return true;
    }

    std::optional<std::string> TclEnvironment::get(const std::string &name) const {
        const std::string key = canonicalVariableName(name);
        const auto it = variables_.find(key);
        if (it == variables_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool TclEnvironment::unset(const std::string &name) {
        const std::string key = canonicalVariableName(name);
        return variables_.erase(key) != 0U;
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
