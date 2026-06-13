add_library(gemini-module-pascal SHARED PascalModule.cpp)
target_link_libraries(gemini-module-pascal PRIVATE gemini-core)
pick_system_enable_warnings(gemini-module-pascal)

if (APPLE)
    set_target_properties(gemini-module-pascal PROPERTIES
            CXX_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF)
endif ()

add_custom_command(
        TARGET gemini-module-pascal
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/src/gemini/modules
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gemini-module-pascal>
                ${CMAKE_BINARY_DIR}/src/gemini/modules/
        COMMENT "Copy gemini-module-pascal into bootstrap modules directory"
)
