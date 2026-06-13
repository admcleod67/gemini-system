add_library(gemini-module-comal SHARED ComalModule.cpp)
target_link_libraries(gemini-module-comal PRIVATE gemini-core)
pick_system_enable_warnings(gemini-module-comal)

if (APPLE)
    set_target_properties(gemini-module-comal PROPERTIES
            CXX_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF)
endif ()

add_custom_command(
        TARGET gemini-module-comal
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/src/gemini/modules
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gemini-module-comal>
                ${CMAKE_BINARY_DIR}/src/gemini/modules/
        COMMENT "Copy gemini-module-comal into bootstrap modules directory"
)
