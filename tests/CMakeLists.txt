include(${CMAKE_SOURCE_DIR}/cmake/PickSystemWarnings.cmake)
include(FetchContent)

FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
)
FetchContent_GetProperties(doctest)
if (NOT doctest_POPULATED)
    FetchContent_Populate(doctest)
endif ()

add_library(doctest_headers INTERFACE)
target_include_directories(doctest_headers SYSTEM INTERFACE ${doctest_SOURCE_DIR})

add_executable(
        pick-tests
        test_Main.cpp
        core/test_BootMonitor.cpp
        core/test_FileSystem.cpp
        core/test_LoginService.cpp
        core/test_Parser.cpp
        core/test_Runtime.cpp
        core/test_VocResolver.cpp
        core/test_MdDefaultDataFile.cpp
        core/test_English.cpp
        core/test_EnglishFormatter.cpp
        basic/test_BasicBytecodeEmitter.cpp
        basic/test_BasicCompiler.cpp
        basic/test_BasicCompilerGolden.cpp
        basic/test_BasicExpressionParser.cpp
        basic/test_BasicSemanticAnalyzer.cpp
        basic/test_BasicStatementParser.cpp
        tcl/test_AssemblerShell.cpp
        tcl/test_Shell.cpp
        tcl/test_TclEnvironment.cpp
        proc/test_ProcShell.cpp
)
target_include_directories(pick-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pick-tests PRIVATE gemini-tcl doctest_headers)
target_compile_definitions(
        pick-tests
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
                "PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE=\"${CMAKE_SOURCE_DIR}/gemini/accounts/SYSPROG\""
)
pick_system_enable_warnings(pick-tests)

if (EXISTS "${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
    include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
    doctest_discover_tests(pick-tests)
else ()
    add_test(NAME pick-unit COMMAND pick-tests)
endif ()

add_test(NAME smoke COMMAND sh -c "echo QUIT | $<TARGET_FILE:gemini-system>")
set_property(TEST smoke PROPERTY ENVIRONMENT "GEMINI_AUTO_LOGON=SYSPROG")
