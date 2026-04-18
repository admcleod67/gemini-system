cmake_minimum_required(VERSION 3.16)

project(
  pick-system
  VERSION 0.1.0
  DESCRIPTION "Pick OS–inspired system (hosted development build)"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build and register tests" ON)

include(GNUInstallDirs)

add_subdirectory(src)

if(BUILD_TESTING)
  include(CTest)
  add_subdirectory(tests)
endif()
