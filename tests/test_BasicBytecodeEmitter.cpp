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
    program.lines.push_back({20, BasicIr::PrintStmt{std::nullopt, makeVar("A"), false}});
    program.lines.push_back({30, BasicIr::EndStmt{}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK(emitted.success);
    CHECK(emitted.errors.empty());
    REQUIRE(emitted.program.size() == 6);
    CHECK(emitted.program[0].op == OpCode::PushInt);
    CHECK(emitted.program[1].op == OpCode::StoreVar);
    CHECK(emitted.program[2].op == OpCode::LoadVar);
    CHECK(emitted.program[3].op == OpCode::PrintInt);
    CHECK(emitted.program[4].op == OpCode::PrintEol);
    CHECK(emitted.program[5].op == OpCode::Halt);
}

TEST_CASE("basic bytecode emitter lowers if and goto control flow") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({10, BasicIr::IfStmt{makeEq(makeInt(1), makeInt(1)), 30, 40}});
    program.lines.push_back({20, BasicIr::StopStmt{}});
    program.lines.push_back({30, BasicIr::GotoStmt{20}});
    program.lines.push_back({40, BasicIr::PrintStmt{std::nullopt, makeInt(99), false}});

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
    program.lines.push_back({10, BasicIr::PrintStmt{std::string("HELLO"), nullptr, true}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK(emitted.success);
    CHECK(emitted.errors.empty());
    REQUIRE(emitted.program.size() == 3);
    CHECK(emitted.program[0].op == OpCode::PushStr);
    CHECK(emitted.program[1].op == OpCode::PrintStr);
    CHECK(emitted.program[2].op == OpCode::Halt);
}
