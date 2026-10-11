add_library(gemini-module-hostfs SHARED HostFsModule.cpp)
target_link_libraries(gemini-module-hostfs PRIVATE gemini-core)
pick_system_enable_warnings(gemini-module-hostfs)

if (APPLE)
    set_target_properties(gemini-module-hostfs PROPERTIES
            CXX_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF)
endif ()

add_custom_command(
        TARGET gemini-module-hostfs
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/src/gemini/modules
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gemini-module-hostfs>
                ${CMAKE_BINARY_DIR}/src/gemini/modules/
        COMMENT "Copy gemini-module-hostfs into bootstrap modules directory"
)
