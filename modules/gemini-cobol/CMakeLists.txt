add_library(gemini-module-cobol SHARED CobolModule.cpp)
target_link_libraries(gemini-module-cobol PRIVATE gemini-core)
pick_system_enable_warnings(gemini-module-cobol)

if (APPLE)
    set_target_properties(gemini-module-cobol PROPERTIES
            CXX_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF)
endif ()

add_custom_command(
        TARGET gemini-module-cobol
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/src/gemini/modules
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gemini-module-cobol>
                ${CMAKE_BINARY_DIR}/src/gemini/modules/
        COMMENT "Copy gemini-module-cobol into bootstrap modules directory"
)
