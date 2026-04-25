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

TEST_CASE("basic bytecode emitter lowers ForStmt to init + ForSetup with default step") {
    BasicIr::NormalizedProgram program;
    BasicIr::ForStmt stmt{};
    stmt.variableName = "I";
    stmt.initExpr = makeInt(1);
    stmt.limitExpr = makeInt(10);
    // stepExpr deliberately left null → emitter should push PUSH_INT 1
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    // Expected: PUSH_INT 1, STORE_VAR I, PUSH_INT 10, PUSH_INT 1 (default step), FOR_SETUP I, HALT
    REQUIRE(emitted.program.size() == 6);
    CHECK(emitted.program[0].op == OpCode::PushInt);
    CHECK(std::get<int>(emitted.program[0].operand) == 1);
    CHECK(emitted.program[1].op == OpCode::StoreVar);
    CHECK(std::get<std::string>(emitted.program[1].operand) == "I");
    CHECK(emitted.program[2].op == OpCode::PushInt);
    CHECK(std::get<int>(emitted.program[2].operand) == 10);
    CHECK(emitted.program[3].op == OpCode::PushInt);  // default step = 1
    CHECK(std::get<int>(emitted.program[3].operand) == 1);
    CHECK(emitted.program[4].op == OpCode::ForSetup);
    CHECK(std::get<std::string>(emitted.program[4].operand) == "I");
    CHECK(emitted.program[5].op == OpCode::Halt);
}

TEST_CASE("basic bytecode emitter lowers ForStmt with explicit STEP") {
    BasicIr::NormalizedProgram program;
    BasicIr::ForStmt stmt{};
    stmt.variableName = "I";
    stmt.initExpr = makeInt(0);
    stmt.limitExpr = makeInt(10);
    stmt.stepExpr = makeInt(2);
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    REQUIRE(emitted.program.size() == 6);
    CHECK(emitted.program[3].op == OpCode::PushInt);
    CHECK(std::get<int>(emitted.program[3].operand) == 2);  // explicit step
    CHECK(emitted.program[4].op == OpCode::ForSetup);
}

TEST_CASE("basic bytecode emitter lowers NextStmt to ForNext opcode") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({20, BasicIr::NextStmt{"I"}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    REQUIRE(emitted.program.size() == 2);
    CHECK(emitted.program[0].op == OpCode::ForNext);
    CHECK(std::get<std::string>(emitted.program[0].operand) == "I");
    CHECK(emitted.program[1].op == OpCode::Halt);
}

TEST_CASE("basic bytecode emitter rejects $ variable as FOR loop variable") {
    BasicIr::NormalizedProgram program;
    BasicIr::ForStmt stmt{};
    stmt.variableName = "I$";
    stmt.initExpr = makeInt(1);
    stmt.limitExpr = makeInt(5);
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK_FALSE(emitted.success);
    REQUIRE(emitted.errors.size() == 1);
    CHECK(emitted.errors[0].line == 10);
}

TEST_CASE("basic bytecode emitter lowers DimStmt to size + DimArray") {
    BasicIr::NormalizedProgram program;
    BasicIr::DimStmt stmt{};
    stmt.variableName = "A";
    stmt.sizeExpr = makeInt(10);
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    // PUSH_INT 10, DIM_ARRAY A, HALT
    REQUIRE(emitted.program.size() == 3);
    CHECK(emitted.program[0].op == OpCode::PushInt);
    CHECK(std::get<int>(emitted.program[0].operand) == 10);
    CHECK(emitted.program[1].op == OpCode::DimArray);
    CHECK(std::get<std::string>(emitted.program[1].operand) == "A");
    CHECK(emitted.program[2].op == OpCode::Halt);
}

TEST_CASE("basic bytecode emitter lowers LetArrayStmt to value + index + StoreArr") {
    BasicIr::NormalizedProgram program;
    BasicIr::LetArrayStmt stmt{};
    stmt.variableName = "A";
    stmt.valueExpr = makeInt(99);
    stmt.indexExpr = makeInt(3);
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    // PUSH_INT 99, PUSH_INT 3, STORE_ARR A, HALT
    REQUIRE(emitted.program.size() == 4);
    CHECK(emitted.program[0].op == OpCode::PushInt);
    CHECK(std::get<int>(emitted.program[0].operand) == 99);
    CHECK(emitted.program[1].op == OpCode::PushInt);
    CHECK(std::get<int>(emitted.program[1].operand) == 3);
    CHECK(emitted.program[2].op == OpCode::StoreArr);
    CHECK(std::get<std::string>(emitted.program[2].operand) == "A");
    CHECK(emitted.program[3].op == OpCode::Halt);
}

TEST_CASE("basic bytecode emitter emits LoadArr for SubscriptExpr in PRINT") {
    BasicIr::NormalizedProgram program;
    // Build PRINT A(2) via a SubscriptExpr
    auto subscript = std::make_unique<BasicAst::Expr>();
    BasicAst::SubscriptExpr subNode{};
    subNode.varName = "A";
    subNode.indexExpr = makeInt(2);
    subscript->node = std::move(subNode);

    BasicIr::PrintStmt printStmt{};
    printStmt.expression = std::move(subscript);
    program.lines.push_back({10, std::move(printStmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    // PUSH_INT 2, LOAD_ARR A, PRINT_VAL, PRINT_EOL, HALT
    REQUIRE(emitted.program.size() == 5);
    CHECK(emitted.program[0].op == OpCode::PushInt);
    CHECK(std::get<int>(emitted.program[0].operand) == 2);
    CHECK(emitted.program[1].op == OpCode::LoadArr);
    CHECK(std::get<std::string>(emitted.program[1].operand) == "A");
    CHECK(emitted.program[2].op == OpCode::PrintVal);
    CHECK(emitted.program[3].op == OpCode::PrintEol);
    CHECK(emitted.program[4].op == OpCode::Halt);
}

TEST_CASE("basic bytecode emitter emits ClearVars for ClearStmt") {
    BasicIr::NormalizedProgram program;
    program.lines.push_back({10, BasicIr::ClearStmt{}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    // CLEAR_VARS, HALT
    REQUIRE(emitted.program.size() == 2);
    CHECK(emitted.program[0].op == OpCode::ClearVars);
    CHECK(emitted.program[1].op == OpCode::Halt);
}

namespace {
    std::unique_ptr<BasicAst::Expr> makeCall(const std::string &name, std::unique_ptr<BasicAst::Expr> arg) {
        auto expr = std::make_unique<BasicAst::Expr>();
        expr->node = BasicAst::FunctionCallExpr{name, std::move(arg), {}};
        return expr;
    }
} // namespace

TEST_CASE("basic bytecode emitter emits argument + AbsInt for ABS") {
    BasicIr::NormalizedProgram program;
    BasicIr::PrintStmt stmt{};
    stmt.expression = makeCall("ABS", makeInt(-5));
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    // PUSH_INT -5 → (emitted as PushInt 0, PushInt 5, Sub), ABS_INT, PRINT_VAL, PRINT_EOL, HALT
    // ABS_INT should appear after the argument expression
    bool found = false;
    for (std::size_t i = 0; i + 1 < emitted.program.size(); ++i) {
        if (emitted.program[i].op == OpCode::AbsInt) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("basic bytecode emitter emits argument + SgnInt for SGN") {
    BasicIr::NormalizedProgram program;
    BasicIr::PrintStmt stmt{};
    stmt.expression = makeCall("SGN", makeInt(3));
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    // PUSH_INT 3, SGN_INT, PRINT_VAL, PRINT_EOL, HALT
    REQUIRE(emitted.program.size() == 5);
    CHECK(emitted.program[0].op == OpCode::PushInt);
    CHECK(std::get<int>(emitted.program[0].operand) == 3);
    CHECK(emitted.program[1].op == OpCode::SgnInt);
    CHECK(emitted.program[2].op == OpCode::PrintVal);
    CHECK(emitted.program[3].op == OpCode::PrintEol);
    CHECK(emitted.program[4].op == OpCode::Halt);
}

TEST_CASE("basic bytecode emitter emits argument + SeqStr for SEQ") {
    BasicIr::NormalizedProgram program;
    BasicIr::PrintStmt stmt{};
    stmt.expression = makeCall("SEQ", makeStr("A"));
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    // PUSH_STR "A", SEQ_STR, PRINT_VAL, PRINT_EOL, HALT
    REQUIRE(emitted.program.size() == 5);
    CHECK(emitted.program[0].op == OpCode::PushStr);
    CHECK(std::get<std::string>(emitted.program[0].operand) == "A");
    CHECK(emitted.program[1].op == OpCode::SeqStr);
    CHECK(emitted.program[2].op == OpCode::PrintVal);
    CHECK(emitted.program[3].op == OpCode::PrintEol);
    CHECK(emitted.program[4].op == OpCode::Halt);
}

TEST_CASE("basic bytecode emitter rejects dollar variable in ABS") {
    BasicIr::NormalizedProgram program;
    BasicIr::PrintStmt stmt{};
    stmt.expression = makeCall("ABS", makeVar("X$"));
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK_FALSE(emitted.success);
}

TEST_CASE("basic bytecode emitter allows dollar variable in SEQ") {
    BasicIr::NormalizedProgram program;
    BasicIr::PrintStmt stmt{};
    stmt.expression = makeCall("SEQ", makeVar("X$"));
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    CHECK(emitted.success);
    // LOAD_VAR X$, SEQ_STR, PRINT_VAL, PRINT_EOL, HALT
    REQUIRE(emitted.program.size() == 5);
    CHECK(emitted.program[0].op == OpCode::LoadVar);
    CHECK(emitted.program[1].op == OpCode::SeqStr);
}

TEST_CASE("basic bytecode emitter emits OpenFile without ELSE") {
    BasicIr::NormalizedProgram program;
    BasicIr::OpenStmt stmt{};
    stmt.fileExpr = makeStr("CUSTOMERS");
    stmt.fileVar = "FVAR";
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    REQUIRE(emitted.program.size() == 3);
    CHECK(emitted.program[0].op == OpCode::PushStr);
    CHECK(emitted.program[1].op == OpCode::OpenFile);
    CHECK(std::get<std::string>(emitted.program[1].operand) == "FVAR");
}

TEST_CASE("basic bytecode emitter emits ReadRec + StoreVar without ELSE") {
    BasicIr::NormalizedProgram program;
    BasicIr::ReadStmt stmt{};
    stmt.targetVar = "REC";
    stmt.fileVar = "FVAR";
    stmt.idExpr = makeVar("ID");
    program.lines.push_back({10, std::move(stmt)});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    CHECK(emitted.program[0].op == OpCode::LoadVar);
    CHECK(emitted.program[1].op == OpCode::ReadRec);
    CHECK(emitted.program[2].op == OpCode::StoreVar);
}

TEST_CASE("basic bytecode emitter emits *_Try opcodes for file ELSE flow") {
    BasicIr::NormalizedProgram program;
    BasicIr::OpenStmt open{};
    open.fileExpr = makeStr("CUST");
    open.fileVar = "FVAR";
    open.elseLine = 40;
    program.lines.push_back({10, std::move(open)});
    program.lines.push_back({40, BasicIr::EndStmt{}});

    const BasicBytecodeEmissionResult emitted = BasicBytecodeEmitter::emit(program);
    REQUIRE(emitted.success);
    bool hasTry = false;
    for (const auto &ins : emitted.program) {
        if (ins.op == OpCode::OpenFileTry) {
            hasTry = true;
            break;
        }
    }
    CHECK(hasTry);
}
