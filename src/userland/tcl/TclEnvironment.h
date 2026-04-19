//
// Created by Allan McLeod on 19/04/2026.
//

#ifndef PICK_SYSTEM_TCL_ENVIRONMENT_H
#define PICK_SYSTEM_TCL_ENVIRONMENT_H

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PickShell {
    // Names are case-insensitive; keys are stored in ASCII uppercase.
    class TclEnvironment {
    public:
        [[nodiscard]] static std::string canonicalVariableName(std::string_view name);

        [[nodiscard]] bool set(std::string name, std::string value);

        [[nodiscard]] std::optional<std::string> get(const std::string &name) const;

        [[nodiscard]] bool unset(const std::string &name);

        [[nodiscard]] std::vector<std::string> names() const;

        void clear();

    private:
        std::unordered_map<std::string, std::string> variables_;
    };
} // namespace PickShell

#endif // PICK_SYSTEM_TCL_ENVIRONMENT_H
