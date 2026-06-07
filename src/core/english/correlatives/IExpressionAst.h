#ifndef PICK_SYSTEM_CORE_ENGLISH_I_EXPRESSION_AST_H
#define PICK_SYSTEM_CORE_ENGLISH_I_EXPRESSION_AST_H

#include <memory>
#include <string>
#include <variant>

namespace PickCore::English {
    struct IExpr;

    struct IExprNumber {
        double value{};
    };

    struct IExprString {
        std::string value;
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

    enum class ICompareOp {
        Eq,
        Ne,
        Lt,
        Le,
        Gt,
        Ge,
    };

    struct IExprCompare {
        ICompareOp op{};
        std::unique_ptr<IExpr> left;
        std::unique_ptr<IExpr> right;
    };

    struct IExprIf {
        std::unique_ptr<IExpr> condition;
        std::unique_ptr<IExpr> thenExpr;
        std::unique_ptr<IExpr> elseExpr;
    };

    enum class ICallKind {
        Iconv,
        Oconv,
    };

    struct IExprCall {
        ICallKind kind{};
        std::unique_ptr<IExpr> valueArg;
        std::unique_ptr<IExpr> codeArg;
    };

    using IExprPayload = std::variant<IExprNumber,
                                      IExprString,
                                      IExprField,
                                      IExprUnary,
                                      IExprBinary,
                                      IExprCompare,
                                      IExprIf,
                                      IExprCall>;

    struct IExpr {
        IExprPayload payload;

        static std::unique_ptr<IExpr> makeNumber(double value);
        static std::unique_ptr<IExpr> makeString(std::string value);
        static std::unique_ptr<IExpr> makeField(int attributeNo);
        static std::unique_ptr<IExpr> makeUnary(char op, std::unique_ptr<IExpr> operand);
        static std::unique_ptr<IExpr> makeBinary(char op,
                                                 std::unique_ptr<IExpr> left,
                                                 std::unique_ptr<IExpr> right);
        static std::unique_ptr<IExpr> makeCompare(ICompareOp op,
                                                  std::unique_ptr<IExpr> left,
                                                  std::unique_ptr<IExpr> right);
        static std::unique_ptr<IExpr> makeIf(std::unique_ptr<IExpr> condition,
                                             std::unique_ptr<IExpr> thenExpr,
                                             std::unique_ptr<IExpr> elseExpr);
        static std::unique_ptr<IExpr> makeCall(ICallKind kind,
                                               std::unique_ptr<IExpr> valueArg,
                                               std::unique_ptr<IExpr> codeArg);
    };
} // namespace PickCore::English

#endif
