add_library(gemini-module-stub SHARED StubModule.cpp)
target_link_libraries(gemini-module-stub PRIVATE gemini-core)
pick_system_enable_warnings(gemini-module-stub)

if (APPLE)
    set_target_properties(gemini-module-stub PROPERTIES
            CXX_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF)
endif ()

add_custom_command(
        TARGET gemini-module-stub
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/src/gemini/modules
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gemini-module-stub>
                ${CMAKE_BINARY_DIR}/src/gemini/modules/
        COMMENT "Copy gemini-module-stub into bootstrap modules directory"
)
