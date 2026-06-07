#include "IExpressionAst.h"

namespace PickCore::English {
    std::unique_ptr<IExpr> IExpr::makeNumber(const double value) {
        auto node = std::make_unique<IExpr>();
        node->payload = IExprNumber{value};
        return node;
    }

    std::unique_ptr<IExpr> IExpr::makeString(std::string value) {
        auto node = std::make_unique<IExpr>();
        node->payload = IExprString{std::move(value)};
        return node;
    }

    std::unique_ptr<IExpr> IExpr::makeField(const int attributeNo) {
        auto node = std::make_unique<IExpr>();
        node->payload = IExprField{attributeNo};
        return node;
    }

    std::unique_ptr<IExpr> IExpr::makeUnary(const char op, std::unique_ptr<IExpr> operand) {
        auto node = std::make_unique<IExpr>();
        IExprUnary unary;
        unary.op = op;
        unary.operand = std::move(operand);
        node->payload = std::move(unary);
        return node;
    }

    std::unique_ptr<IExpr> IExpr::makeBinary(const char op,
                                             std::unique_ptr<IExpr> left,
                                             std::unique_ptr<IExpr> right) {
        auto node = std::make_unique<IExpr>();
        IExprBinary binary;
        binary.op = op;
        binary.left = std::move(left);
        binary.right = std::move(right);
        node->payload = std::move(binary);
        return node;
    }

    std::unique_ptr<IExpr> IExpr::makeCompare(const ICompareOp op,
                                              std::unique_ptr<IExpr> left,
                                              std::unique_ptr<IExpr> right) {
        auto node = std::make_unique<IExpr>();
        IExprCompare compare;
        compare.op = op;
        compare.left = std::move(left);
        compare.right = std::move(right);
        node->payload = std::move(compare);
        return node;
    }

    std::unique_ptr<IExpr> IExpr::makeIf(std::unique_ptr<IExpr> condition,
                                         std::unique_ptr<IExpr> thenExpr,
                                         std::unique_ptr<IExpr> elseExpr) {
        auto node = std::make_unique<IExpr>();
        IExprIf conditional;
        conditional.condition = std::move(condition);
        conditional.thenExpr = std::move(thenExpr);
        conditional.elseExpr = std::move(elseExpr);
        node->payload = std::move(conditional);
        return node;
    }

    std::unique_ptr<IExpr> IExpr::makeCall(const ICallKind kind,
                                           std::unique_ptr<IExpr> valueArg,
                                           std::unique_ptr<IExpr> codeArg) {
        auto node = std::make_unique<IExpr>();
        IExprCall call;
        call.kind = kind;
        call.valueArg = std::move(valueArg);
        call.codeArg = std::move(codeArg);
        node->payload = std::move(call);
        return node;
    }
} // namespace PickCore::English
