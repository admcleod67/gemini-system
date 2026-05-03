include(${CMAKE_SOURCE_DIR}/cmake/PickSystemWarnings.cmake)

add_subdirectory(core)
add_subdirectory(userland)

add_custom_target(
        gemini-bootstrap-copy
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/gemini
                ${CMAKE_CURRENT_BINARY_DIR}/gemini
        COMMENT "Copy gemini bootstrap tree next to built executables"
)

add_executable(gemini-system Main.cpp)
target_link_libraries(gemini-system PRIVATE gemini-tcl)
pick_system_enable_warnings(gemini-system)
add_dependencies(gemini-system gemini-bootstrap-copy)

add_executable(gemini-cli userland/cli/Main.cpp)
target_link_libraries(gemini-cli PRIVATE gemini-tcl)
pick_system_enable_warnings(gemini-cli)
add_dependencies(gemini-cli gemini-bootstrap-copy)

target_compile_definitions(
        gemini-system
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

install(TARGETS gemini-system gemini-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
