#include "BasicCompileUtils.h"

#include <cctype>

namespace PickShell {
    std::string uppercase(std::string s) {
        for (char &c: s) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return s;
    }

    bool isValidVariableName(const std::string &token) {
        if (token.empty()) {
            return false;
        }
        const char first = token.front();
        if (std::isalpha(static_cast<unsigned char>(first)) == 0) {
            return false;
        }
        for (const char c: token) {
            if (std::isalnum(static_cast<unsigned char>(c)) == 0) {
                return false;
            }
        }
        return true;
    }
} // namespace PickShell
