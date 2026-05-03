add_library(gemini-core
        vm/Runtime.cpp
        vm/Runtime.h
        vm/Parser.cpp
        vm/Parser.h
        vm/InstructionPrint.cpp
        vm/InstructionPrint.h
        vm/BytecodeText.cpp
        vm/BytecodeText.h
        filesystem/Record.cpp
        filesystem/Record.h
        filesystem/Catalog.cpp
        filesystem/Catalog.h
        filesystem/FileSystem.cpp
        filesystem/FileSystem.h
        voc/VocResolver.cpp
        voc/VocResolver.h
        catalog/GeminiCatalog.cpp
        catalog/GeminiCatalog.h
        login/LoginService.cpp
        login/LoginService.h
        login/UserSession.h
        host/HostBootstrap.cpp
        host/HostBootstrap.h
        boot/BootMonitor.cpp
        boot/BootMonitor.h
)

target_include_directories(
        gemini-core
        PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/vm"
        "${CMAKE_CURRENT_SOURCE_DIR}/filesystem"
        "${CMAKE_CURRENT_SOURCE_DIR}/voc"
        "${CMAKE_CURRENT_SOURCE_DIR}/catalog"
        "${CMAKE_CURRENT_SOURCE_DIR}/login"
        "${CMAKE_CURRENT_SOURCE_DIR}/host"
        "${CMAKE_CURRENT_SOURCE_DIR}/boot"
)

target_link_libraries(gemini-core PUBLIC nlohmann_json::nlohmann_json)

target_compile_definitions(
        gemini-core
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

pick_system_enable_warnings(gemini-core)
