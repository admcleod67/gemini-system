add_library(pick-tcl
        tcl/Shell.cpp
        tcl/Shell.h
)

target_include_directories(
        pick-tcl
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/tcl"
)

target_link_libraries(pick-tcl PUBLIC pick-core)

target_compile_definitions(
        pick-tcl
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

pick_system_enable_warnings(pick-tcl)
