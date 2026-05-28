#include "CorrelativeEvaluator.h"

namespace PickCore::English {
    namespace {
        constexpr const char *kConversionNotSupported = "F-type conversion not supported";
    } // namespace

    std::optional<std::string> CorrelativeEvaluator::evaluateF(const FCorrelativeDef &def,
                                                               const PickFS::StructuredRecord &dataRecord,
                                                               std::string &error) {
        error.clear();

        if (def.selectorKind == FSelectorKind::ConversionRaw) {
            error = kConversionNotSupported;
            return std::nullopt;
        }

        const int attrNo = def.sourceAttributeNo;
        if (!dataRecord.hasAttribute(attrNo)) {
            error = "F-type: attribute " + std::to_string(attrNo) + " missing";
            return std::nullopt;
        }

        const PickFS::RecordAttribute &attr = dataRecord.attribute(attrNo);

        switch (def.selectorKind) {
            case FSelectorKind::ValueIndex: {
                if (def.valueIndex <= 0) {
                    error = "F-type: invalid value index";
                    return std::nullopt;
                }
                return attr.valueAt(def.valueIndex);
            }
            case FSelectorKind::Leftmost:
                return attr.firstValue();
            case FSelectorKind::Rightmost: {
                const std::vector<std::string> values = attr.splitValues();
                if (values.empty()) {
                    return std::string{};
                }
                return values.back();
            }
            case FSelectorKind::ConversionRaw:
                error = kConversionNotSupported;
                return std::nullopt;
        }

        error = kConversionNotSupported;
        return std::nullopt;
    }
} // namespace PickCore::English
