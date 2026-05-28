#include "IExpressionAst.h"

namespace PickCore::English {
    std::unique_ptr<IExpr> IExpr::makeNumber(const double value) {
        auto node = std::make_unique<IExpr>();
        node->payload = IExprNumber{value};
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
} // namespace PickCore::English
