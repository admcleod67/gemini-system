include(${CMAKE_SOURCE_DIR}/cmake/PickSystemWarnings.cmake)

add_subdirectory(core)
add_subdirectory(userland)

add_executable(gemini-system Main.cpp)
target_link_libraries(gemini-system PRIVATE pick-tcl)
pick_system_enable_warnings(gemini-system)

add_executable(pick-cli userland/cli/Main.cpp)
target_link_libraries(pick-cli PRIVATE pick-tcl)
pick_system_enable_warnings(pick-cli)

target_compile_definitions(
        gemini-system
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

install(TARGETS gemini-system pick-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
