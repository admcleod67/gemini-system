add_library(gemini-core
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
        gemini-core
        PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/vm"
        "${CMAKE_CURRENT_SOURCE_DIR}/filesystem"
)

target_compile_definitions(
        gemini-core
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

pick_system_enable_warnings(gemini-core)
