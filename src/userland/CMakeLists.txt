add_library(gemini-tcl
        assembler/AssemblerShell.cpp
        assembler/AssemblerShell.h
        assembler/VmDebugService.cpp
        assembler/VmDebugService.h
        basic/BasicProgram.cpp
        basic/BasicProgram.h
        basic/BasicAst.h
        basic/BasicCompileUtils.cpp
        basic/BasicCompileUtils.h
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
        proc/ProcInterpreter.cpp
        proc/ProcInterpreter.h
        tcl/LineRecordEditor.cpp
        tcl/LineRecordEditor.h
        tcl/HelpTopics.cpp
        tcl/HelpTopics.h
        tcl/Shell.cpp
        tcl/Shell.h
        tcl/DefaultFileSystemRoot.cpp
        tcl/DefaultFileSystemRoot.h
        tcl/GeminiSession.cpp
        tcl/GeminiSession.h
        tcl/GeminiSessionHost.cpp
        tcl/GeminiSessionHost.h
        tcl/SessionTable.cpp
        tcl/SessionTable.h
        tcl/TclEnvironment.cpp
        tcl/TclEnvironment.h
)

target_include_directories(
        gemini-tcl
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/assembler"
        "${CMAKE_CURRENT_SOURCE_DIR}/basic"
        "${CMAKE_CURRENT_SOURCE_DIR}/proc"
        "${CMAKE_CURRENT_SOURCE_DIR}/tcl"
)

target_link_libraries(gemini-tcl PUBLIC gemini-core)

target_compile_definitions(
        gemini-tcl
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

pick_system_enable_warnings(gemini-tcl)
