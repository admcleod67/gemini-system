add_library(pick-tcl
        basic/BasicProgram.cpp
        basic/BasicProgram.h
        basic/BasicAst.h
        basic/BasicShell.cpp
        basic/BasicShell.h
        basic/BasicCompiler.cpp
        basic/BasicCompiler.h
        basic/BasicExpressionParser.cpp
        basic/BasicExpressionParser.h
        basic/BasicNormalizedIr.h
        basic/BasicSemanticAnalyzer.cpp
        basic/BasicSemanticAnalyzer.h
        basic/BasicBytecodeEmitter.cpp
        basic/BasicBytecodeEmitter.h
        basic/BasicStatementParser.cpp
        basic/BasicStatementParser.h
        tcl/Shell.cpp
        tcl/Shell.h
        tcl/ShellSession.cpp
        tcl/ShellSession.h
        tcl/TclEnvironment.cpp
        tcl/TclEnvironment.h
)

target_include_directories(
        pick-tcl
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/basic"
        "${CMAKE_CURRENT_SOURCE_DIR}/tcl"
)

target_link_libraries(pick-tcl PUBLIC pick-core)

target_compile_definitions(
        pick-tcl
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

pick_system_enable_warnings(pick-tcl)
