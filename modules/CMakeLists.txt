add_subdirectory(gemini-stub)
add_subdirectory(gemini-basic)
add_subdirectory(gemini-pascal)
add_subdirectory(gemini-comal)
add_subdirectory(gemini-cobol)

# Install built language modules into the shared Runtime bootstrap tree (M17 Stage 6).
# COMPONENT must be listed per artifact type (or use DESTINATION+COMPONENT together).
foreach (module_target IN ITEMS
        gemini-module-stub
        gemini-module-basic
        gemini-module-pascal
        gemini-module-comal
        gemini-module-cobol)
    install(TARGETS ${module_target}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/gemini/modules
            COMPONENT Runtime)
endforeach ()
