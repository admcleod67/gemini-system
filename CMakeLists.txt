cmake_minimum_required(VERSION 3.16)

if (POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif ()

project(
        gemini-system
        VERSION 0.1.0
        DESCRIPTION "Gemini System inspired by Pick OS (hosted development build)"
        LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS
        ON
        CACHE BOOL "Write compile_commands.json for Clang tooling and IDEs" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build and register tests" ON)

include(GNUInstallDirs)

add_subdirectory(src)

if (BUILD_TESTING)
    include(CTest)
    add_subdirectory(tests)
endif ()
