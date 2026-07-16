#include <doctest/doctest.h>

#include "Parser.h"
#include "Runtime.h"

#include <sstream>
#include <string>

using PickVM::LoadedBytecode;
using PickVM::OpCode;
using PickVM::Parser;
using PickVM::Runtime;

namespace {
    std::string helloTbcPath() {
#if defined(GEMINI_TEST_HELLO_TBC)
        return GEMINI_TEST_HELLO_TBC;
#else
        return {};
#endif
    }
} // namespace

TEST_CASE("standalone vm parseFile hello.tbc and run with console output") {
    const std::string path = helloTbcPath();
    REQUIRE_FALSE(path.empty());

    Parser parser;
    const LoadedBytecode loaded = parser.parseFile(path);
    REQUIRE_FALSE(loaded.program.empty());
    CHECK(loaded.program.back().op == OpCode::Halt);

    // PRINT_* only — no CALL_FUNC, no file opcodes; FS intentionally unbound.
    for (const auto &instr : loaded.program) {
        CHECK(instr.op != OpCode::CallFunc);
        CHECK(instr.op != OpCode::OpenFile);
        CHECK(instr.op != OpCode::ReadRec);
        CHECK(instr.op != OpCode::WriteRec);
    }

    Runtime runtime;
    // Do not call setFileSystem — standalone runner leaves FS null.
    std::ostringstream out;
    runtime.setOutputStream(&out);
    runtime.loadProgram(loaded.program, loaded.sourceLinePerInstr);
    runtime.run();

    CHECK(out.str() == "Hello, world\n");
}

TEST_CASE("standalone vm parseFile missing file throws") {
    Parser parser;
    CHECK_THROWS_WITH(parser.parseFile("/nonexistent/gemini-vm/no_such_file.tbc"),
                      doctest::Contains("Cannot open bytecode file"));
}
