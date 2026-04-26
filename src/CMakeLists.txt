include(${CMAKE_SOURCE_DIR}/cmake/PickSystemWarnings.cmake)

add_subdirectory(core)
add_subdirectory(userland)

add_executable(gemini-system Main.cpp)
target_link_libraries(gemini-system PRIVATE gemini-tcl)
pick_system_enable_warnings(gemini-system)

add_executable(gemini-cli userland/cli/Main.cpp)
target_link_libraries(gemini-cli PRIVATE gemini-tcl)
pick_system_enable_warnings(gemini-cli)

target_compile_definitions(
        gemini-system
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

install(TARGETS gemini-system gemini-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
