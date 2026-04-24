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
        test_Parser.cpp
        test_Runtime.cpp
        test_BasicCompiler.cpp
        test_BasicExpressionParser.cpp
        test_BasicStatementParser.cpp
        test_Shell.cpp
        test_TclEnvironment.cpp
)
target_link_libraries(pick-tests PRIVATE pick-tcl doctest_headers)
target_compile_definitions(
        pick-tests
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)
pick_system_enable_warnings(pick-tests)

if (EXISTS "${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
    include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
    doctest_discover_tests(pick-tests)
else ()
    add_test(NAME pick-unit COMMAND pick-tests)
endif ()

add_test(NAME smoke COMMAND sh -c "echo QUIT | $<TARGET_FILE:pick-system>")
