#include "CorrelativeEvaluator.h"

#include "../../conversion/PickOconv.h"
#include "IExpressionEvaluator.h"

namespace PickCore::English {
    std::optional<std::string> CorrelativeEvaluator::evaluateF(const FCorrelativeDef &def,
                                                               const PickFS::StructuredRecord &dataRecord,
                                                               std::string &error) {
        error.clear();

        const int attrNo = def.sourceAttributeNo;
        if (!dataRecord.hasAttribute(attrNo)) {
            error = "F-type: attribute " + std::to_string(attrNo) + " missing";
            return std::nullopt;
        }

        const PickFS::RecordAttribute &attr = dataRecord.attribute(attrNo);

        if (def.selectorKind == FSelectorKind::ConversionRaw) {
            return Conversion::oconvOutput(attr.firstValue(), def.tailRaw, error);
        }

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
        }

        error = "F-type: invalid selector";
        return std::nullopt;
    }

    std::optional<std::string> CorrelativeEvaluator::evaluateI(const ICorrelativeDef &def,
                                                               const PickFS::StructuredRecord &dataRecord,
                                                               std::string &error) {
        error.clear();
        if (!def.expression) {
            error = "I-type: invalid expression";
            return std::nullopt;
        }
        return IExpressionEvaluator::evaluate(*def.expression, dataRecord, error);
    }

    std::string CorrelativeEvaluator::evaluateFieldCell(const FieldRef &ref,
                                                        const PickFS::StructuredRecord &dataRecord) {
        if (ref.kind == DictFieldKind::FCorrelative) {
            if (!ref.fCorrelative.has_value()) {
                return {};
            }
            std::string error;
            if (const std::optional<std::string> value =
                    evaluateF(*ref.fCorrelative, dataRecord, error)) {
                return *value;
            }
            return {};
        }

        if (ref.kind == DictFieldKind::ICorrelative) {
            if (!ref.iCorrelative.has_value()) {
                return {};
            }
            std::string error;
            if (const std::optional<std::string> value =
                    evaluateI(*ref.iCorrelative, dataRecord, error)) {
                return *value;
            }
            return {};
        }

        if (!ref.attributeNo.has_value()) {
            return {};
        }
        return dataRecord.attribute(*ref.attributeNo).firstValue();
    }
} // namespace PickCore::English
