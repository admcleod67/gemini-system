include(${CMAKE_SOURCE_DIR}/cmake/PickSystemWarnings.cmake)
include(FetchContent)

FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
)
FetchContent_GetProperties(doctest)
if (NOT doctest_POPULATED)
    FetchContent_Populate(doctest)
endif ()

add_library(doctest_headers INTERFACE)
target_include_directories(doctest_headers SYSTEM INTERFACE ${doctest_SOURCE_DIR})

add_executable(
        pick-tests
        test_Main.cpp
        TestProcessReset.cpp
        core/test_BootMonitor.cpp
        core/test_GeminiDaemon.cpp
        core/test_DaemonConfig.cpp
        core/test_DaemonLifecycle.cpp
        core/test_SystemdPackaging.cpp
        core/test_DaemonIpc.cpp
        core/test_DaemonIpcSession.cpp
        core/test_SessionIoBridge.cpp
        console/test_GeminiConsole.cpp
        core/test_PortManager.cpp
        core/test_SessionTable.cpp
        core/test_CooperativeSessionRunner.cpp
        core/test_SchedulableSessionIo.cpp
        core/test_FileSystem.cpp
        core/test_LoginService.cpp
        core/test_Parser.cpp
        core/test_Runtime.cpp
        core/test_VocResolver.cpp
        core/test_MdDefaultDataFile.cpp
        core/test_English.cpp
        core/test_EnglishFormatter.cpp
        core/test_CorrelativeEvaluator.cpp
        core/test_PickOconv.cpp
        core/test_PickIconv.cpp
        core/test_LockTable.cpp
        core/test_LanguageRegistry.cpp
        core/test_CallFunc.cpp
        core/test_LanguageAbi.cpp
        core/test_LanguageRegistryFormat.cpp
        core/test_LanguageModuleLoader.cpp
        core/test_FileSystemLocking.cpp
        basic/test_BasicBytecodeEmitter.cpp
        basic/test_BasicCompiler.cpp
        basic/test_BasicCompilerGolden.cpp
        basic/test_BasicExpressionParser.cpp
        basic/test_BasicSemanticAnalyzer.cpp
        basic/test_BasicStatementParser.cpp
        tcl/test_AssemblerShell.cpp
        tcl/test_GeminiSession.cpp
        tcl/test_Shell.cpp
        tcl/test_ShellLocking.cpp
        tcl/test_M10CrossShell.cpp
        tcl/test_GeminiSessionLocks.cpp
        tcl/test_TclEnvironment.cpp
        proc/test_ProcShell.cpp
        proc/test_ProcLocking.cpp
)
target_include_directories(pick-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_link_libraries(pick-tests PRIVATE gemini-tcl doctest_headers)
add_dependencies(pick-tests gemini-module-stub gemini-module-basic gemini-module-pascal gemini-module-comal
        gemini-module-cobol)
target_compile_definitions(
        pick-tests
        PRIVATE "PICK_SYSTEM_VERSION=\"${PROJECT_VERSION}\""
                "PICK_SYSTEM_GEMINI_SYSPROG_FIXTURE=\"${CMAKE_SOURCE_DIR}/gemini/accounts/SYSPROG\""
                "PICK_SYSTEM_GEMINI_SERVICE_UNIT=\"${CMAKE_BINARY_DIR}/packaging/systemd/gemini.service\""
                "PICK_SYSTEM_GEMINI_DAEMON_CONF=\"${CMAKE_SOURCE_DIR}/packaging/gemini/daemon.conf\""
                "GEMINI_STUB_MODULE_PATH=\"$<TARGET_FILE:gemini-module-stub>\""
                "GEMINI_BASIC_MODULE_PATH=\"$<TARGET_FILE:gemini-module-basic>\""
                "GEMINI_PASCAL_MODULE_PATH=\"$<TARGET_FILE:gemini-module-pascal>\""
                "GEMINI_COMAL_MODULE_PATH=\"$<TARGET_FILE:gemini-module-comal>\""
                "GEMINI_COBOL_MODULE_PATH=\"$<TARGET_FILE:gemini-module-cobol>\""
)
pick_system_enable_warnings(pick-tests)

if (EXISTS "${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
    include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
    doctest_discover_tests(pick-tests)
else ()
    add_test(NAME pick-unit COMMAND pick-tests)
endif ()

add_test(NAME smoke COMMAND sh -c "echo QUIT | $<TARGET_FILE:gemini-system>")
set_property(TEST smoke PROPERTY ENVIRONMENT "GEMINI_AUTO_LOGON=SYSPROG")
