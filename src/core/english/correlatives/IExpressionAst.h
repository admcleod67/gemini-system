#ifndef PICK_SYSTEM_CORE_ENGLISH_I_EXPRESSION_AST_H
#define PICK_SYSTEM_CORE_ENGLISH_I_EXPRESSION_AST_H

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace PickCore::English {
    struct IExpr;

    struct IExprNumber {
        double value{};
    };

    struct IExprField {
        /// 1-based attribute index (A=1, B=2, …).
        int attributeNo{};
    };

    struct IExprUnary {
        char op{}; // '-'
        std::unique_ptr<IExpr> operand;
    };

    struct IExprBinary {
        char op{}; // '+', '-', '*', '/'
        std::unique_ptr<IExpr> left;
        std::unique_ptr<IExpr> right;
    };

    using IExprPayload = std::variant<IExprNumber, IExprField, IExprUnary, IExprBinary>;

    struct IExpr {
        IExprPayload payload;

        static std::unique_ptr<IExpr> makeNumber(double value);
        static std::unique_ptr<IExpr> makeField(int attributeNo);
        static std::unique_ptr<IExpr> makeUnary(char op, std::unique_ptr<IExpr> operand);
        static std::unique_ptr<IExpr> makeBinary(char op,
                                                 std::unique_ptr<IExpr> left,
                                                 std::unique_ptr<IExpr> right);
    };
} // namespace PickCore::English

#endif
