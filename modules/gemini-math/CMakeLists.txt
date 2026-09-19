add_library(gemini-module-math SHARED MathModule.cpp)
target_link_libraries(gemini-module-math PRIVATE gemini-core)
pick_system_enable_warnings(gemini-module-math)

if (APPLE)
    set_target_properties(gemini-module-math PROPERTIES
            CXX_VISIBILITY_PRESET default
            VISIBILITY_INLINES_HIDDEN OFF)
endif ()

add_custom_command(
        TARGET gemini-module-math
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/src/gemini/modules
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gemini-module-math>
                ${CMAKE_BINARY_DIR}/src/gemini/modules/
        COMMENT "Copy gemini-module-math into bootstrap modules directory"
)
