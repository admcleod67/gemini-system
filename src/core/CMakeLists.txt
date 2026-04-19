add_library(pick-core
        vm/Runtime.cpp
        vm/Runtime.h
        vm/Parser.cpp
        vm/Parser.h
        vm/InstructionPrint.cpp
        vm/InstructionPrint.h
        filesystem/Record.cpp
        filesystem/Record.h
        filesystem/Catalog.cpp
        filesystem/Catalog.h
        filesystem/FileSystem.cpp
        filesystem/FileSystem.h
)

target_include_directories(
        pick-core
        PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/vm"
        "${CMAKE_CURRENT_SOURCE_DIR}/filesystem"
)

target_compile_definitions(
        pick-core
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

pick_system_enable_warnings(pick-core)
