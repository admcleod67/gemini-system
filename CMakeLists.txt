cmake_minimum_required(VERSION 3.16)

if (POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif ()

project(
        gemini-system
        VERSION 1.0.0
        DESCRIPTION "Gemini System inspired by Pick OS (hosted development build)"
        LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS
        ON
        CACHE BOOL "Write compile_commands.json for Clang tooling and IDEs" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build and register tests" ON)

include(GNUInstallDirs)
include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Install components (Milestone 17 Stage 6):
#   Runtime     — share/gemini bootstrap + language modules
#   Application — gemini-system
#   Service     — gemini-daemon, gemini-console, systemd unit, daemon.conf
# Plain `cmake --install` installs all components. Edition recipes use --component.

add_subdirectory(src)
add_subdirectory(packaging)

if (BUILD_TESTING)
    include(CTest)
    add_subdirectory(tests)
endif ()
