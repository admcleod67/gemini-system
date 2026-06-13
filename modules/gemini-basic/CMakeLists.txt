add_library(gemini-module-basic SHARED BasicModule.cpp)
target_link_libraries(gemini-module-basic PRIVATE gemini-core)
pick_system_enable_warnings(gemini-module-basic)

if (APPLE)
    set_target_properties(gemini-module-basic PROPERTIES
            CXX_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF)
endif ()

add_custom_command(
        TARGET gemini-module-basic
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/src/gemini/modules
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gemini-module-basic>
                ${CMAKE_BINARY_DIR}/src/gemini/modules/
        COMMENT "Copy gemini-module-basic into bootstrap modules directory"
)
