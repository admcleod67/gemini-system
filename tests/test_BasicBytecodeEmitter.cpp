#include <doctest/doctest.h>

#include "BasicBytecodeEmitter.h"
#include "BasicNormalizedIr.h"
#include "Runtime.h"

#include <optional>
#include <string>

using PickShell::BasicBytecodeEmitter;
using PickShell::BasicBytecodeEmissionResult;
namespace BasicAst = PickShell::BasicAst;
namespace BasicIr = PickShell::BasicIr;
using PickVM::OpCode;

namespace {
    std::unique_ptr<BasicAst::Expr> makeInt(const int value) {
        auto expr = std::make_unique<BasicAst::Expr>();
        expr->node = BasicAst::IntLiteralExpr{value, {}};
        return expr;
    }

    std::unique_ptr<BasicAst::Expr> makeStr(const std::string &value) {
        auto expr = std::make_unique<BasicAst::Expr>();
        expr->node = BasicAst::StringLiteralExpr{value, {}};
        return expr;
    }

    std::unique_ptr<BasicAst::Expr> makeVar(const std::string &name) {
        auto expr = std::make_unique<BasicAst::Expr>();
        expr->node = BasicAst::IdentifierExpr{name, {}};
        return expr;
    }

    std::unique_ptr<BasicAst::Expr> makeEq(std::unique_ptr<BasicAst::Expr> lhs, std::unique_ptr<BasicAst::Expr> rhs) {
        auto expr = std::make_unique<BasicAst::Expr>();
        expr->node = BasicAst::BinaryExpr{BasicAst::BinaryOp::Equal, std::move(lhs), std::move(rhs), {}};
        return expr;
    }
} // namespace

TEST_CASE("basic bytecode emitter lowers let print and end") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({10, BasicIr::LetStmt{"A", makeInt(5)}});
    program.lines.push_back({20, BasicIr::PrintStmt{makeVar("A"), false}});
    program.lines.push_back({30, BasicIr::EndStmt{}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK(emitted.success);
    CHECK(emitted.errors.empty());
    REQUIRE(emitted.program.size() == 6);
    CHECK(emitted.program[0].op == OpCode::PushInt);
    CHECK(emitted.program[1].op == OpCode::StoreVar);
    CHECK(emitted.program[2].op == OpCode::LoadVar);
    CHECK(emitted.program[3].op == OpCode::PrintVal);
    CHECK(emitted.program[4].op == OpCode::PrintEol);
    CHECK(emitted.program[5].op == OpCode::Halt);
}

TEST_CASE("basic bytecode emitter lowers if and goto control flow") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({10, BasicIr::IfStmt{makeEq(makeInt(1), makeInt(1)), 30, 40}});
    program.lines.push_back({20, BasicIr::StopStmt{}});
    program.lines.push_back({30, BasicIr::GotoStmt{20}});
    program.lines.push_back({40, BasicIr::PrintStmt{makeInt(99), false}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK(emitted.success);
    CHECK(emitted.errors.empty());
    REQUIRE(emitted.program.size() >= 10);
    CHECK(emitted.program[2].op == OpCode::Eq);
    CHECK(emitted.program[3].op == OpCode::JumpIfZero);
    CHECK(emitted.program[4].op == OpCode::Jump);
    CHECK(emitted.program[5].op == OpCode::Jump);
    CHECK(emitted.program[6].op == OpCode::Halt);
    CHECK(emitted.program[7].op == OpCode::Jump);
}

TEST_CASE("basic bytecode emitter preserves print string and suppress eol") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({10, BasicIr::PrintStmt{makeStr("HELLO"), true}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK(emitted.success);
    CHECK(emitted.errors.empty());
    REQUIRE(emitted.program.size() == 3);
    CHECK(emitted.program[0].op == OpCode::PushStr);
    CHECK(emitted.program[1].op == OpCode::PrintVal);
    CHECK(emitted.program[2].op == OpCode::Halt);
}

TEST_CASE("basic bytecode emitter rejects LET with null expression") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({10, BasicIr::LetStmt{"A", nullptr}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK_FALSE(emitted.success);
    REQUIRE(emitted.errors.size() == 1);
    CHECK(emitted.errors[0].line == 10);
    CHECK(emitted.errors[0].message == "LET requires an expression");
    CHECK(emitted.program.empty());
}

TEST_CASE("basic bytecode emitter rejects IF with null condition") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({10, BasicIr::IfStmt{nullptr, 20, std::nullopt}});
    program.lines.push_back({20, BasicIr::EndStmt{}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK_FALSE(emitted.success);
    REQUIRE(emitted.errors.size() == 1);
    CHECK(emitted.errors[0].message == "IF requires a condition expression");
    CHECK(emitted.program.empty());
}

TEST_CASE("basic bytecode emitter rejects PRINT numeric path with null expression") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({10, BasicIr::PrintStmt{nullptr, false}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK_FALSE(emitted.success);
    REQUIRE(emitted.errors.size() == 1);
    CHECK(emitted.errors[0].message == "PRINT requires an expression");
    CHECK(emitted.program.empty());
}

TEST_CASE("basic bytecode emitter rejects malformed expression nodes") {
    BasicIr::NormalizedProgram unaryProgram;
    auto unaryExpr = std::make_unique<BasicAst::Expr>();
    unaryExpr->node = BasicAst::UnaryExpr{BasicAst::UnaryOp::Negate, nullptr, {}};
    unaryProgram.lines.push_back({10, BasicIr::LetStmt{"A", std::move(unaryExpr)}});
    const auto unaryResult = BasicBytecodeEmitter::emit(unaryProgram);
    CHECK_FALSE(unaryResult.success);
    REQUIRE(unaryResult.errors.size() == 1);
    CHECK(unaryResult.errors[0].message == "LET expression error: Invalid unary expression");

    BasicIr::NormalizedProgram binaryProgram;
    auto binaryExpr = std::make_unique<BasicAst::Expr>();
    binaryExpr->node = BasicAst::BinaryExpr{BasicAst::BinaryOp::Add, nullptr, nullptr, {}};
    binaryProgram.lines.push_back({10, BasicIr::LetStmt{"A", std::move(binaryExpr)}});
    const auto binaryResult = BasicBytecodeEmitter::emit(binaryProgram);
    CHECK_FALSE(binaryResult.success);
    REQUIRE(binaryResult.errors.size() == 1);
    CHECK(binaryResult.errors[0].message == "LET expression error: Invalid binary expression");

    BasicIr::NormalizedProgram groupedProgram;
    auto groupedExpr = std::make_unique<BasicAst::Expr>();
    groupedExpr->node = BasicAst::GroupedExpr{nullptr, {}};
    groupedProgram.lines.push_back({10, BasicIr::LetStmt{"A", std::move(groupedExpr)}});
    const auto groupedResult = BasicBytecodeEmitter::emit(groupedProgram);
    CHECK_FALSE(groupedResult.success);
    REQUIRE(groupedResult.errors.size() == 1);
    CHECK(groupedResult.errors[0].message == "LET expression error: Invalid grouped expression");
}

TEST_CASE("basic bytecode emitter rejects invalid identifier in expression path") {
    BasicIr::NormalizedProgram program;
    auto expr = std::make_unique<BasicAst::Expr>();
    expr->node = BasicAst::IdentifierExpr{"1BAD", {}};
    program.lines.push_back({10, BasicIr::PrintStmt{std::move(expr), false}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK_FALSE(emitted.success);
    REQUIRE(emitted.errors.size() == 1);
    CHECK(emitted.errors[0].message == "PRINT expression error: Invalid variable name '1BAD'");
    CHECK(emitted.program.empty());
}

TEST_CASE("basic bytecode emitter lowers GosubStmt to Call opcode with fixup") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({10, BasicIr::GosubStmt{30}});
    program.lines.push_back({20, BasicIr::EndStmt{}});
    program.lines.push_back({30, BasicIr::ReturnStmt{}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK(emitted.success);
    CHECK(emitted.errors.empty());
    // line 10: CALL (target=ip of line 30)
    REQUIRE(emitted.program.size() >= 3);
    CHECK(emitted.program[0].op == OpCode::Call);
    // Fixup must have resolved the Call operand to the IP of line 30
    const int callTarget = std::get<int>(emitted.program[0].operand);
    CHECK(callTarget == 2); // line 30 is the 3rd line → ip 2
    // line 20: HALT
    CHECK(emitted.program[1].op == OpCode::Halt);
    // line 30: RETURN
    CHECK(emitted.program[2].op == OpCode::Return);
}

TEST_CASE("basic bytecode emitter lowers ReturnStmt to Return opcode") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({10, BasicIr::ReturnStmt{}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK(emitted.success);
    REQUIRE(emitted.program.size() == 2);
    CHECK(emitted.program[0].op == OpCode::Return);
    CHECK(emitted.program[1].op == OpCode::Halt);
}
