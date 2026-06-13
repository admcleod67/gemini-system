#include "BasicBuiltinHandlers.h"

#include "BasicBuiltinSupport.h"
#include "Runtime.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace PickCore::Languages::Basic {
    namespace {
        PickVM::Runtime *runtimeFromHost(void *const hostContext) {
            return static_cast<PickVM::Runtime *>(hostContext);
        }

        thread_local std::mt19937 gRndEngine{std::random_device{}()};

        void pushRndSample(std::vector<PickVM::Value> &stack) {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            stack.push_back(dist(gRndEngine));
        }
    } // namespace

    void handlerAbs(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        stack.push_back(std::abs(coerceToInt(v)));
    }

    void handlerSgn(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        const int x = coerceToInt(v);
        stack.push_back(x > 0 ? 1 : x < 0 ? -1 : 0);
    }

    void handlerSeq(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        const std::string s = valueToString(v);
        stack.push_back(s.empty() ? 0 : static_cast<int>(static_cast<unsigned char>(s[0])));
    }

    void handlerLen(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        stack.push_back(static_cast<int>(valueToString(v).size()));
    }

    void handlerTrim(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        const std::string s = valueToString(v);
        const std::size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            stack.push_back(std::string{});
        } else {
            const std::size_t last = s.find_last_not_of(" \t\r\n");
            stack.push_back(s.substr(first, last - first + 1));
        }
    }

    void handlerLcase(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        std::string s = valueToString(v);
        for (char &c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        stack.push_back(std::move(s));
    }

    void handlerUcase(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        std::string s = valueToString(v);
        for (char &c : s) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        stack.push_back(std::move(s));
    }

    void handlerSpace(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        int n = coerceToInt(v);
        if (n < 0) {
            n = 0;
        }
        if (n > kBuiltinSpaceMaxCount) {
            throw std::runtime_error("BUILTIN: SPACE count exceeds limit");
        }
        stack.push_back(std::string(static_cast<std::size_t>(n), ' '));
    }

    void handlerInt(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        const double d = coerceToDouble(v);
        stack.push_back(static_cast<int>(d));
    }

    void handlerMod(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value bVal = stack.back();
        stack.pop_back();
        const PickVM::Value aVal = stack.back();
        stack.pop_back();
        const double b = coerceToDouble(bVal);
        const double a = coerceToDouble(aVal);
        if (b == 0.0) {
            throw std::runtime_error("BUILTIN: MOD division by zero");
        }
        stack.push_back(std::fmod(a, b));
    }

    void handlerSin(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        stack.push_back(std::sin(coerceToDouble(v)));
    }

    void handlerCos(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        stack.push_back(std::cos(coerceToDouble(v)));
    }

    void handlerTan(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        stack.push_back(std::tan(coerceToDouble(v)));
    }

    void handlerExp(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        stack.push_back(std::exp(coerceToDouble(v)));
    }

    void handlerLog(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        const double x = coerceToDouble(v);
        if (x <= 0.0) {
            throw std::runtime_error("BUILTIN: LOG domain");
        }
        stack.push_back(std::log(x));
    }

    void handlerDate(std::vector<PickVM::Value> &stack, void *) {
        const std::time_t now = std::time(nullptr);
        const auto dayUnix = static_cast<int>(now / 86400LL);
        stack.push_back(dayUnix + kPickInternalDateAtUnixEpoch);
    }

    void handlerTime(std::vector<PickVM::Value> &stack, void *) {
        const std::time_t now = std::time(nullptr);
        std::tm tmBuf{};
#if defined(_WIN32)
        gmtime_s(&tmBuf, &now);
#else
        gmtime_r(&now, &tmBuf);
#endif
        char buf[16]{};
        if (std::strftime(buf, sizeof buf, "%H:%M:%S", &tmBuf) == 0) {
            stack.push_back(std::string{"00:00:00"});
        } else {
            stack.push_back(std::string{buf});
        }
    }

    void handlerSystem(std::vector<PickVM::Value> &stack, void *const hostContext) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        const int n = coerceToInt(v);
        PickVM::Runtime *const rt = runtimeFromHost(hostContext);
        stack.push_back(rt->evalBuiltinSystemCall(n));
    }

    void handlerIndex(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value occVal = stack.back();
        stack.pop_back();
        const PickVM::Value needleVal = stack.back();
        stack.pop_back();
        const PickVM::Value hayVal = stack.back();
        stack.pop_back();
        const std::string hay = valueToString(hayVal);
        const std::string needle = valueToString(needleVal);
        if (needle.empty()) {
            throw std::runtime_error("BUILTIN: INDEX empty substring");
        }
        const int occ = coerceToInt(occVal);
        if (occ < 1) {
            throw std::runtime_error("BUILTIN: INDEX occurrence");
        }
        std::size_t pos = 0;
        for (int i = 1; i <= occ; ++i) {
            const std::size_t found = hay.find(needle, pos);
            if (found == std::string::npos) {
                stack.push_back(0);
                return;
            }
            if (i == occ) {
                stack.push_back(static_cast<int>(found + 1));
                return;
            }
            pos = found + 1;
        }
        stack.push_back(0);
    }

    void handlerField(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value fieldIdxVal = stack.back();
        stack.pop_back();
        const PickVM::Value delimVal = stack.back();
        stack.pop_back();
        const PickVM::Value hayVal = stack.back();
        stack.pop_back();
        const std::string hay = valueToString(hayVal);
        const std::string delim = valueToString(delimVal);
        if (delim.empty()) {
            throw std::runtime_error("BUILTIN: FIELD empty delimiter");
        }
        const int fieldNum = coerceToInt(fieldIdxVal);
        if (fieldNum < 1) {
            throw std::runtime_error("BUILTIN: FIELD field index");
        }
        std::vector<std::string> fields;
        std::size_t start = 0;
        for (;;) {
            const std::size_t f = hay.find(delim, start);
            if (f == std::string::npos) {
                fields.push_back(hay.substr(start));
                break;
            }
            fields.push_back(hay.substr(start, f - start));
            start = f + delim.size();
        }
        if (static_cast<std::size_t>(fieldNum) > fields.size()) {
            stack.push_back(std::string{});
        } else {
            stack.push_back(fields[static_cast<std::size_t>(fieldNum - 1)]);
        }
    }

    void handlerStr(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        stack.push_back(valueToString(v));
    }

    void handlerOconv(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value codeVal = stack.back();
        stack.pop_back();
        const PickVM::Value val = stack.back();
        stack.pop_back();
        stack.push_back(dispatchConvertCode(ConvertDirection::Output, val, valueToString(codeVal)));
    }

    void handlerIconv(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value codeVal = stack.back();
        stack.pop_back();
        const PickVM::Value val = stack.back();
        stack.pop_back();
        stack.push_back(dispatchConvertCode(ConvertDirection::Input, val, valueToString(codeVal)));
    }

    void handlerNum(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value v = stack.back();
        stack.pop_back();
        bool numeric = false;
        if (std::holds_alternative<int>(v) || std::holds_alternative<double>(v)) {
            numeric = true;
        } else {
            numeric = isFullyNumeric(std::get<std::string>(v));
        }
        stack.push_back(numeric ? 1 : 0);
    }

    void handlerConvert(std::vector<PickVM::Value> &stack, void *) {
        const PickVM::Value toVal = stack.back();
        stack.pop_back();
        const PickVM::Value fromVal = stack.back();
        stack.pop_back();
        const PickVM::Value srcVal = stack.back();
        stack.pop_back();
        const std::string src = valueToString(srcVal);
        const std::string from = valueToString(fromVal);
        const std::string to = valueToString(toVal);
        std::string out;
        out.reserve(src.size());
        for (const char c : src) {
            const std::size_t pos = from.find(c);
            if (pos == std::string::npos) {
                out += c;
            } else if (pos < to.size()) {
                out += to[pos];
            }
        }
        stack.push_back(std::move(out));
    }

    void handlerRnd0(std::vector<PickVM::Value> &stack, void *) {
        pushRndSample(stack);
    }

    void handlerRnd1(std::vector<PickVM::Value> &stack, void *) {
        gRndEngine.seed(static_cast<unsigned>(coerceToInt(stack.back())));
        stack.pop_back();
        pushRndSample(stack);
    }

    void handlerStatus(std::vector<PickVM::Value> &stack, void *const hostContext) {
        const PickVM::Runtime *const rt = runtimeFromHost(hostContext);
        stack.push_back(rt != nullptr ? rt->statusCode() : 0);
    }
} // namespace PickCore::Languages::Basic
