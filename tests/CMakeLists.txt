include(${CMAKE_SOURCE_DIR}/cmake/PickSystemWarnings.cmake)
include(FetchContent)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
)
FetchContent_GetProperties(doctest)
if(NOT doctest_POPULATED)
  FetchContent_Populate(doctest)
endif()

add_library(doctest_headers INTERFACE)
target_include_directories(doctest_headers INTERFACE ${doctest_SOURCE_DIR})

add_executable(
  pick-tests
  test_main.cpp
  test_parser.cpp
  test_runtime.cpp
  test_shell.cpp
)
target_include_directories(
  pick-tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/vm
    ${CMAKE_SOURCE_DIR}/src/tcl
)
target_link_libraries(pick-tests PRIVATE pick-core doctest_headers)
target_compile_definitions(
  pick-tests
  PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)
pick_system_enable_warnings(pick-tests)

if(EXISTS "${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
  include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
  doctest_discover_tests(pick-tests)
else()
  add_test(NAME pick-unit COMMAND pick-tests)
endif()

add_test(NAME smoke COMMAND pick-system)
