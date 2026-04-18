add_library(pick-core
        core/vm/VM.cpp
        core/vm/VM.h
        core/vm/Instructions.cpp
        core/vm/Instructions.h
        tcl/tcl.cpp
        tcl/tcl.h
)

target_include_directories(
  pick-core
  PUBLIC ${CMAKE_SOURCE_DIR}/include
)

add_executable(pick-system main.cpp)
target_link_libraries(pick-system PRIVATE pick-core)

add_executable(pick-cli core/cli/main.cpp)
target_link_libraries(pick-cli PRIVATE pick-core)

target_compile_definitions(
  pick-system
  PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
)

install(TARGETS pick-system pick-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
