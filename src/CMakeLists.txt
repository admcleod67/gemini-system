include(${CMAKE_SOURCE_DIR}/cmake/PickSystemWarnings.cmake)

add_subdirectory(core)
add_subdirectory(userland)

add_executable(pick-system Main.cpp)
target_link_libraries(pick-system PRIVATE pick-tcl)
pick_system_enable_warnings(pick-system)

add_executable(pick-cli core/cli/Main.cpp)
target_link_libraries(pick-cli PRIVATE pick-tcl)
pick_system_enable_warnings(pick-cli)

target_compile_definitions(
        pick-system
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

install(TARGETS pick-system pick-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
