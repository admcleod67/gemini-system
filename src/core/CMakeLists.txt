add_library(pick-core
        vm/Runtime.cpp
        vm/Runtime.h
        vm/Parser.cpp
        vm/Parser.h
        vm/InstructionPrint.cpp
        vm/InstructionPrint.h
)

target_include_directories(
        pick-core
        PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/vm"
)

target_compile_definitions(
        pick-core
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

pick_system_enable_warnings(pick-core)
