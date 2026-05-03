#include "BasicCompileUtils.h"

#include <cctype>

namespace PickShell {
    std::string uppercase(std::string s) {
        for (char &c: s) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return s;
    }

    bool isReadonlySessionSystemVariable(const std::string &token) {
        const std::string u = uppercase(token);
        return u == "@USERNO" || u == "@ACCOUNT" || u == "@LOGNAME";
    }

    bool isValidVariableName(const std::string &token) {
        if (token.empty()) {
            return false;
        }
        if (token.front() == '@') {
            return isReadonlySessionSystemVariable(token);
        }
        const char first = token.front();
        if (std::isalpha(static_cast<unsigned char>(first)) == 0) {
            return false;
        }
        // Allow optional $ or % suffix for string/integer variables; body chars must be alphanumeric.
        const std::size_t bodyEnd = (token.size() > 1 && (token.back() == '$' || token.back() == '%'))
                                        ? token.size() - 1
                                        : token.size();
        for (std::size_t i = 0; i < bodyEnd; ++i) {
            if (std::isalnum(static_cast<unsigned char>(token[i])) == 0) {
                return false;
            }
        }
        return true;
    }
} // namespace PickShell
