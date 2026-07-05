include(${CMAKE_SOURCE_DIR}/cmake/PickSystemWarnings.cmake)

add_subdirectory(core)
add_subdirectory(userland)
add_subdirectory(${CMAKE_SOURCE_DIR}/modules ${CMAKE_BINARY_DIR}/modules)

add_custom_target(
        gemini-bootstrap-copy
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/gemini
                ${CMAKE_CURRENT_BINARY_DIR}/gemini
        COMMENT "Copy gemini bootstrap tree next to built executables"
)

add_executable(gemini-system Main.cpp)
target_link_libraries(gemini-system PRIVATE gemini-tcl)
pick_system_enable_warnings(gemini-system)
add_dependencies(gemini-system gemini-bootstrap-copy gemini-module-stub gemini-module-basic
        gemini-module-pascal gemini-module-comal gemini-module-cobol)

target_compile_definitions(
        gemini-system
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

install(TARGETS gemini-system RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(gemini-daemon daemon/Main.cpp)
target_link_libraries(gemini-daemon PRIVATE gemini-tcl)
pick_system_enable_warnings(gemini-daemon)
add_dependencies(gemini-daemon gemini-bootstrap-copy gemini-module-stub gemini-module-basic
        gemini-module-pascal gemini-module-comal gemini-module-cobol)

target_compile_definitions(
        gemini-daemon
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

install(TARGETS gemini-daemon RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(gemini-console console/Main.cpp)
target_link_libraries(gemini-console PRIVATE gemini-core)
pick_system_enable_warnings(gemini-console)

install(TARGETS gemini-console RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
