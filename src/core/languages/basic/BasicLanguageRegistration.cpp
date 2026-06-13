#include "BasicLanguageRegistration.h"

#include "BasicBuiltinHandlers.h"
#include "BasicLanguageIds.h"
#include "LanguageRegistry.h"

namespace PickCore::Languages::Basic {
    void registerBasicLanguage(LanguageRegistry &registry, void *const hostContext) {
        LanguageNamespaceDescriptor descriptor{};
        descriptor.id = kNamespaceId;
        descriptor.metadata.name = "basic";
        descriptor.metadata.version = "1";
        descriptor.functions = {
            LanguageFunctionEntry{1, handlerAbs},
            LanguageFunctionEntry{1, handlerSgn},
            LanguageFunctionEntry{1, handlerSeq},
            LanguageFunctionEntry{1, handlerLen},
            LanguageFunctionEntry{1, handlerTrim},
            LanguageFunctionEntry{1, handlerLcase},
            LanguageFunctionEntry{1, handlerUcase},
            LanguageFunctionEntry{1, handlerSpace},
            LanguageFunctionEntry{1, handlerInt},
            LanguageFunctionEntry{2, handlerMod},
            LanguageFunctionEntry{1, handlerSin},
            LanguageFunctionEntry{1, handlerCos},
            LanguageFunctionEntry{1, handlerTan},
            LanguageFunctionEntry{1, handlerExp},
            LanguageFunctionEntry{1, handlerLog},
            LanguageFunctionEntry{0, handlerDate},
            LanguageFunctionEntry{0, handlerTime},
            LanguageFunctionEntry{1, handlerSystem},
            LanguageFunctionEntry{3, handlerIndex},
            LanguageFunctionEntry{3, handlerField},
            LanguageFunctionEntry{1, handlerStr},
            LanguageFunctionEntry{2, handlerOconv},
            LanguageFunctionEntry{2, handlerIconv},
            LanguageFunctionEntry{1, handlerNum},
            LanguageFunctionEntry{3, handlerConvert},
            LanguageFunctionEntry{0, handlerRnd0},
            LanguageFunctionEntry{1, handlerRnd1},
            LanguageFunctionEntry{0, handlerStatus},
        };
        registry.registerNamespace(std::move(descriptor), hostContext);
    }
} // namespace PickCore::Languages::Basic
