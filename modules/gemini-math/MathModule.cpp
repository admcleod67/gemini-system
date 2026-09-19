#include "LanguageModuleAbi.h"
#include "LanguageRegistry.h"
#include "LanguageTypes.h"

#include <gemini/math_function_ids.hpp>
#include <gemini/namespace_ids.hpp>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {
    double coerceToDouble(const PickVM::Value &v) {
        if (std::holds_alternative<int>(v)) {
            return static_cast<double>(std::get<int>(v));
        }
        if (std::holds_alternative<double>(v)) {
            return std::get<double>(v);
        }
        const std::string &s = std::get<std::string>(v);
        if (s.empty()) {
            return 0.0;
        }
        char *endp = nullptr;
        errno = 0;
        const double n = std::strtod(s.c_str(), &endp);
        if (endp == s.c_str() || errno != 0) {
            return 0.0;
        }
        return n;
    }

    PickVM::Value popOne(std::vector<PickVM::Value> &stack) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        return v;
    }

    void handlerSqrt(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        const double x = coerceToDouble(popOne(stack));
        if (x < 0.0) {
            throw std::runtime_error("MATH: SQRT domain");
        }
        stack.push_back(std::sqrt(x));
    }

    void handlerSin(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        stack.push_back(std::sin(coerceToDouble(popOne(stack))));
    }

    void handlerCos(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        stack.push_back(std::cos(coerceToDouble(popOne(stack))));
    }

    void handlerTan(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        stack.push_back(std::tan(coerceToDouble(popOne(stack))));
    }

    void handlerArctan(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        stack.push_back(std::atan(coerceToDouble(popOne(stack))));
    }

    void handlerLn(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        const double x = coerceToDouble(popOne(stack));
        if (x <= 0.0) {
            throw std::runtime_error("MATH: LN domain");
        }
        stack.push_back(std::log(x));
    }

    void handlerExp(std::vector<PickVM::Value> &stack, void * /*hostContext*/) {
        stack.push_back(std::exp(coerceToDouble(popOne(stack))));
    }
} // namespace

GEMINI_LANGUAGE_MODULE_EXPORT void register_language(PickCore::Languages::LanguageRegistry &registry) {
    PickCore::Languages::LanguageNamespaceDescriptor descriptor{};
    descriptor.id = Gemini::kNamespaceIdMath;
    descriptor.metadata.name = "math";
    descriptor.metadata.version = "1";
    descriptor.functions = {
        PickCore::Languages::LanguageFunctionEntry{1, handlerSqrt},   // Gemini::Math::kFnSqrt
        PickCore::Languages::LanguageFunctionEntry{1, handlerSin},    // Gemini::Math::kFnSin
        PickCore::Languages::LanguageFunctionEntry{1, handlerCos},    // Gemini::Math::kFnCos
        PickCore::Languages::LanguageFunctionEntry{1, handlerTan},    // Gemini::Math::kFnTan
        PickCore::Languages::LanguageFunctionEntry{1, handlerArctan}, // Gemini::Math::kFnArctan
        PickCore::Languages::LanguageFunctionEntry{1, handlerLn},     // Gemini::Math::kFnLn
        PickCore::Languages::LanguageFunctionEntry{1, handlerExp},    // Gemini::Math::kFnExp
    };
    static_assert(Gemini::Math::kFunctionCount == 7);
    registry.registerNamespace(std::move(descriptor));
}
