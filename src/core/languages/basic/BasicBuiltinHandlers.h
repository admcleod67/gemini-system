#ifndef PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_HANDLERS_H
#define PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_HANDLERS_H

#include "../LanguageTypes.h"

namespace PickCore::Languages::Basic {
    void handlerAbs(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerSgn(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerSeq(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerLen(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerTrim(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerLcase(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerUcase(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerSpace(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerInt(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerMod(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerSin(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerCos(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerTan(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerExp(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerLog(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerDate(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerTime(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerSystem(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerIndex(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerField(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerStr(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerOconv(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerIconv(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerNum(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerConvert(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerRnd0(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerRnd1(std::vector<PickVM::Value> &stack, void *hostContext);
    void handlerStatus(std::vector<PickVM::Value> &stack, void *hostContext);
} // namespace PickCore::Languages::Basic

#endif // PICK_SYSTEM_CORE_LANGUAGES_BASIC_BASIC_BUILTIN_HANDLERS_H
