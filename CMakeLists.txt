cmake_minimum_required(VERSION 3.24)

# ------------------------------------------------------------------------------
# Project Definition & Language Initialization
# ------------------------------------------------------------------------------
project(
    PolyglotCLI
    VERSION 1.0.0
    LANGUAGES C CXX
    DESCRIPTION "Cross-platform CLI with unified C/C++/Fortran spdlog dual-sink logging"
)

# Standardize language levels
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers and tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensure position-independent code across all targets
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optional Fortran 2008 subsystem detection
option(ENABLE_FORTRAN "Enable Fortran 2008 subsystem" ON)
if(ENABLE_FORTRAN)
    include(CheckLanguage)
    check_language(Fortran)
    if(CMAKE_Fortran_COMPILER)
        enable_language(Fortran)
        set(HAVE_FORTRAN TRUE)
        set(CMAKE_Fortran_STANDARD 2008)
        set(CMAKE_Fortran_STANDARD_REQUIRED ON)
        set(CMAKE_Fortran_EXTENSIONS OFF)
        set(CMAKE_Fortran_MODULE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fortran_modules)
        message(STATUS "Fortran compiler detected: ${CMAKE_Fortran_COMPILER_ID} ${CMAKE_Fortran_COMPILER_VERSION}")
    else()
        set(HAVE_FORTRAN FALSE)
        message(STATUS "No Fortran compiler detected. Building C and C++ subsystems only.")
    endif()
endif()

option(POLYGLOT_BUILD_STATIC "Build monolithic static binaries with static runtime" OFF)
if(POLYGLOT_BUILD_STATIC AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ------------------------------------------------------------------------------
# Dependency Management: spdlog (System vs FetchContent)
# ------------------------------------------------------------------------------
option(POLYGLOT_USE_SYSTEM_SPDLOG "Search for system installed spdlog before FetchContent" OFF)
set(POLYGLOT_SPDLOG_VERSION "v1.14.1" CACHE STRING "spdlog git release tag")

if(POLYGLOT_USE_SYSTEM_SPDLOG)
    find_package(spdlog CONFIG QUIET)
endif()

if(NOT TARGET spdlog::spdlog)
    include(FetchContent)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE INTERNAL "Disable spdlog examples")
    set(SPDLOG_BUILD_TESTS OFF CACHE INTERNAL "Disable spdlog tests")
    set(SPDLOG_INSTALL OFF CACHE INTERNAL "Disable spdlog install rules")
    set(SPDLOG_BUILD_SHARED OFF CACHE INTERNAL "Build static spdlog")

    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        ${POLYGLOT_SPDLOG_VERSION}
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# ------------------------------------------------------------------------------
# Target 1: C++ Logging Core & C ABI Wrapper (Static Library)
# ------------------------------------------------------------------------------
add_library(polyglot_logger_core STATIC
    src/polyglot_log.cpp
)

target_include_directories(polyglot_logger_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(polyglot_logger_core
    PRIVATE
        spdlog::spdlog
)

# ------------------------------------------------------------------------------
# Target 2: C Subsystem (Pure C numerical component)
# ------------------------------------------------------------------------------
add_library(c_subsystem STATIC
    src/c_subsystem.c
)

target_include_directories(c_subsystem
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(c_subsystem
    PUBLIC
        polyglot_logger_core
)

# ------------------------------------------------------------------------------
# Target 3: Fortran 2008 Interoperability Module (Conditional Static Library)
# ------------------------------------------------------------------------------
if(HAVE_FORTRAN)
    add_library(polyglot_logger_fortran STATIC
        src/logging_mod.f90
    )

    target_include_directories(polyglot_logger_fortran
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_Fortran_MODULE_DIRECTORY}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(polyglot_logger_fortran
        PUBLIC
            polyglot_logger_core
    )

    # Enable Fortran preprocessor for __FILE__ and __LINE__ source-location capture
    if(CMAKE_Fortran_COMPILER_ID MATCHES "GNU")
        target_compile_options(polyglot_logger_fortran PRIVATE -cpp -ffree-line-length-none)
        # When C++ linker is not GNU g++ (e.g. clang++ or Intel icpx), explicitly link gfortran runtime
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            target_link_libraries(polyglot_logger_fortran INTERFACE gfortran)
        endif()
    elseif(CMAKE_Fortran_COMPILER_ID MATCHES "Intel|IntelLLVM")
        if(WIN32)
            target_compile_options(polyglot_logger_fortran PRIVATE /fpp)
            if(POLYGLOT_BUILD_STATIC)
                target_compile_options(polyglot_logger_fortran PRIVATE /libs:static /threads)
            endif()
        else()
            target_compile_options(polyglot_logger_fortran PRIVATE -fpp)
        endif()
    endif()
endif()

# ------------------------------------------------------------------------------
# Target 4: Main CLI Application Executable
# ------------------------------------------------------------------------------
add_executable(cli_tool
    src/main.cpp
)

set_target_properties(cli_tool PROPERTIES
    OUTPUT_NAME "polyglot-cli"
    LINKER_LANGUAGE CXX
)

if(POLYGLOT_BUILD_STATIC)
    if(MSVC)
        target_link_options(cli_tool PRIVATE /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:msvcrtd.lib)
    elseif(MINGW)
        target_link_options(cli_tool PRIVATE -static -static-libgcc -static-libstdc++)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(cli_tool PRIVATE -static-libgcc -static-libstdc++)
        if(HAVE_FORTRAN AND CMAKE_Fortran_COMPILER_ID MATCHES "GNU")
            target_link_options(cli_tool PRIVATE -static-libgfortran)
        endif()
    endif()
endif()

target_link_libraries(cli_tool
    PRIVATE
        c_subsystem
        polyglot_logger_core
)

if(HAVE_FORTRAN)
    target_link_libraries(cli_tool
        PRIVATE
            polyglot_logger_fortran
    )
    target_compile_definitions(cli_tool
        PRIVATE
            ENABLE_FORTRAN_SUBSYSTEM=1
    )
endif()

# ------------------------------------------------------------------------------
# Target 5: Multi-threaded Concurrency Test Executable
# ------------------------------------------------------------------------------
add_executable(test_multithreaded
    tests/test_multithreaded.cpp
)

target_link_libraries(test_multithreaded
    PRIVATE
        polyglot_logger_core
)

set_target_properties(test_multithreaded PROPERTIES
    LINKER_LANGUAGE CXX
)

# ------------------------------------------------------------------------------
# Standalone Examples Integration
# ------------------------------------------------------------------------------
option(BUILD_EXAMPLES "Build standalone zero-dependency example projects" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ------------------------------------------------------------------------------
# Compiler Diagnostics & Strict Warning Profiles
# ------------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(polyglot_logger_core PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor
    )
    target_compile_options(c_subsystem PRIVATE
        -Wall -Wextra -Wpedantic -Wmissing-prototypes -Wstrict-prototypes
    )
    target_compile_options(cli_tool PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow
    )
    target_compile_options(test_multithreaded PRIVATE
        -Wall -Wextra -Wpedantic
    )
elseif(MSVC)
    target_compile_options(polyglot_logger_core PRIVATE
        /W4 /permissive- /utf-8
    )
    target_compile_options(c_subsystem PRIVATE
        /W4 /utf-8
    )
    target_compile_options(cli_tool PRIVATE
        /W4 /permissive- /utf-8
    )
    target_compile_options(test_multithreaded PRIVATE
        /W4 /permissive- /utf-8
    )
endif()

if(HAVE_FORTRAN)
    if(CMAKE_Fortran_COMPILER_ID MATCHES "GNU")
        target_compile_options(polyglot_logger_fortran PRIVATE
            -Wall -Wextra -Wimplicit-interface -fimplicit-none
        )
    elseif(CMAKE_Fortran_COMPILER_ID MATCHES "Intel|IntelLLVM")
        if(WIN32)
            target_compile_options(polyglot_logger_fortran PRIVATE
                /warn:all /stand:f08
            )
        else()
            target_compile_options(polyglot_logger_fortran PRIVATE
                -warn all -stand f08
            )
        endif()
    endif()
endif()

# ------------------------------------------------------------------------------
# Installation Rules & Reusable CMake Package Export
# ------------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS cli_tool
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Application
)

# Export libraries for downstream find_package(PolyglotLog)
set(EXPORT_TARGETS polyglot_logger_core)
if(TARGET spdlog)
    get_target_property(_is_imported spdlog IMPORTED)
    if(NOT _is_imported)
        list(APPEND EXPORT_TARGETS spdlog)
    endif()
endif()
if(HAVE_FORTRAN)
    list(APPEND EXPORT_TARGETS polyglot_logger_fortran)
endif()

install(TARGETS ${EXPORT_TARGETS}
    EXPORT PolyglotLogTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Development
)

install(FILES
    include/polyglot_log.h
    include/polyglot_log.hpp
    include/c_subsystem.h
    include/fortran_solver.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT Development
)

if(HAVE_FORTRAN)
    install(DIRECTORY ${CMAKE_Fortran_MODULE_DIRECTORY}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT Development
        FILES_MATCHING PATTERN "*.mod"
    )
endif()

install(EXPORT PolyglotLogTargets
    FILE PolyglotLogTargets.cmake
    NAMESPACE PolyglotLog::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PolyglotLog
    COMPONENT Development
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PolyglotLogConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Create PolyglotLogConfig.cmake
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/PolyglotLogConfig.cmake.in"
"@PACKAGE_INIT@
include(\"\${CMAKE_CURRENT_LIST_DIR}/PolyglotLogTargets.cmake\")
check_required_components(PolyglotLog)
"
)

configure_package_config_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PolyglotLogConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/PolyglotLogConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PolyglotLog
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PolyglotLogConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PolyglotLogConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PolyglotLog
    COMPONENT Development
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT Documentation
    OPTIONAL
)

# ------------------------------------------------------------------------------
# CTest Configuration
# ------------------------------------------------------------------------------
include(CTest)

if(BUILD_TESTING)
    set(TEST_DRIVER "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_cli_test.cmake")

    # 1. STDOUT Isolation (Application data must be clean with zero log leakage)
    add_test(
        NAME test_stdout_data_cleanliness
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DEXPECT_STDOUT=^COMPUTATION_SUCCESS: 42$
            -DFORBID_STDOUT=(\\[info\\]|\\[debug\\]|\\[trace\\])
            -P ${TEST_DRIVER}
    )

    # 2. Default Log Level (stderr must contain [info], but NOT [debug] or [trace])
    add_test(
        NAME test_default_verbosity_filtering
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DEXPECT_STDERR=\\[info\\]
            -DFORBID_STDERR=(\\[debug\\]|\\[trace\\])
            -P ${TEST_DRIVER}
    )

    # 3. Verbose Flag (stderr must show [debug] output when -v is passed)
    add_test(
        NAME test_verbose_debug_flag
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DCLI_ARGS=-v
            -DEXPECT_STDERR=\\[debug\\]
            -DFORBID_STDERR=\\[trace\\]
            -P ${TEST_DRIVER}
    )

    # 4. Trace Flag (stderr must show [trace] output when -vv is passed)
    add_test(
        NAME test_trace_verbosity_flag
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DCLI_ARGS=-vv
            -DEXPECT_STDERR=\\[trace\\]
            -P ${TEST_DRIVER}
    )

    # 5. Quiet Flag (stderr must suppress [info], [debug], and [trace])
    add_test(
        NAME test_quiet_mode_flag
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DCLI_ARGS=-q
            -DFORBID_STDERR=(\\[info\\]|\\[debug\\]|\\[trace\\])
            -P ${TEST_DRIVER}
    )

    # 6. Silent Flag (--silent disables all console logging)
    add_test(
        NAME test_silent_mode_flag
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DCLI_ARGS=--silent
            -DFORBID_STDERR=.+
            -P ${TEST_DRIVER}
    )

    # 7. Explicit Named Level Flag (--log-level debug)
    add_test(
        NAME test_named_log_level_flag
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            "-DCLI_ARGS=--log-level debug"
            -DEXPECT_STDERR=\\[debug\\]
            -P ${TEST_DRIVER}
    )

    # 8. Dual-Sink File Logging (Verify disk write, ISO timestamp, thread info, source loc)
    set(TEST_LOG_PATH "${CMAKE_CURRENT_BINARY_DIR}/test_execution.log")
    add_test(
        NAME test_dual_sink_file_generation
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            "-DCLI_ARGS=-l ${TEST_LOG_PATH}"
            -DEXPECT_STDERR=\\[info\\]
            -DEXPECT_FILE=${TEST_LOG_PATH}
            "-DEXPECT_FILE_REGEX=[0-9]+-[0-9]+-[0-9]+T.*Application starting"
            -P ${TEST_DRIVER}
    )

    # 9. Independent File Level (--log-file-level info suppresses trace/debug in file)
    set(TEST_INFO_LOG "${CMAKE_CURRENT_BINARY_DIR}/test_info.log")
    add_test(
        NAME test_file_level_independence
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            "-DCLI_ARGS=-l ${TEST_INFO_LOG} --log-file-level info"
            -DEXPECT_FILE=${TEST_INFO_LOG}
            "-DEXPECT_FILE_REGEX=\\[info\\].*Application starting"
            -P ${TEST_DRIVER}
    )

    # 10. Environment Variable Override (POLYGLOT_LOG_LEVEL=debug)
    add_test(
        NAME test_env_var_override
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DTEST_ENV=POLYGLOT_LOG_LEVEL=debug
            -DEXPECT_STDERR=\\[debug\\]
            -P ${TEST_DRIVER}
    )

    # 11. NO_COLOR Standard Compliance (NO_COLOR=1)
    add_test(
        NAME test_no_color_compliance
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DTEST_ENV=NO_COLOR=1
            -DEXPECT_STDERR=\\[info\\]
            -P ${TEST_DRIVER}
    )

    # 12. Conflicting Flags Rejection (-q and -v together must fail with non-zero exit)
    add_test(
        NAME test_conflicting_flags_error
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            "-DCLI_ARGS=-q -v"
            -DEXPECT_EXIT_CODE=1
            -DEXPECT_STDERR=Contradictory
            -P ${TEST_DRIVER}
    )

    # 13. Help Flag (Emits usage and exits 0)
    add_test(
        NAME test_help_flag
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DCLI_ARGS=--help
            -DEXPECT_STDOUT=Usage:.*--help
            -P ${TEST_DRIVER}
    )

    # 14. Polyglot Subsystem Logging (C-Subsystem and Fortran-Solver logs present)
    if(HAVE_FORTRAN)
        add_test(
            NAME test_polyglot_solver_logging
            COMMAND ${CMAKE_COMMAND}
                -DCLI_EXE=$<TARGET_FILE:cli_tool>
                -DCLI_ARGS=-v
                -DEXPECT_STDERR=(\\[C-Subsystem\\]|\\[Fortran-Solver\\])
                -P ${TEST_DRIVER}
        )
    endif()

    # 15. Multi-threaded Concurrency Stress Test (Explicitly writes 1,600 logs to disk!)
    set(TEST_THREAD_LOG "${CMAKE_CURRENT_BINARY_DIR}/test_multithreaded.log")
    add_test(
        NAME test_multithreaded_concurrency
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:test_multithreaded>
            "-DCLI_ARGS=${TEST_THREAD_LOG}"
            -DEXPECT_STDOUT=MULTITHREADED_TEST_SUCCESS
            -DEXPECT_FILE=${TEST_THREAD_LOG}
            "-DEXPECT_FILE_REGEX=Worker 7.*199"
            -P ${TEST_DRIVER}
    )

    # 16. Color Mode Flags (--color=never and --color=always)
    add_test(
        NAME test_color_mode_flags
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DCLI_ARGS=--color=never
            -DEXPECT_STDERR=\\[info\\]
            -P ${TEST_DRIVER}
    )

    # 17. Structured JSON Log Format (--log-format=json)
    set(TEST_JSON_LOG "${CMAKE_CURRENT_BINARY_DIR}/test_json.log")
    add_test(
        NAME test_json_log_format
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            "-DCLI_ARGS=-l ${TEST_JSON_LOG} --log-format=json"
            -DEXPECT_FILE=${TEST_JSON_LOG}
            "-DEXPECT_FILE_REGEX=\\{\"timestamp\":\".*\",\"pid\":.*,\"level\":\"info\""
            -P ${TEST_DRIVER}
    )

    # 18. Positional Arguments & End-of-Options Delimiter (--)
    add_test(
        NAME test_positional_arguments_and_delimiter
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            "-DCLI_ARGS=-v -- input.mesh output.h5"
            -DEXPECT_STDOUT=^COMPUTATION_SUCCESS: 42$
            -DEXPECT_STDERR=\\[debug\\].*Received 2 positional arguments
            -P ${TEST_DRIVER}
    )

    # 19. Failure-Path Validation: Invalid Log Level Flag
    add_test(
        NAME test_invalid_log_level_error
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            "-DCLI_ARGS=--log-level nonsense"
            -DEXPECT_EXIT_CODE=1
            -DEXPECT_STDERR=Invalid log level
            -P ${TEST_DRIVER}
    )

    # 20. Standalone Examples Stream Isolation Tests
    if(BUILD_EXAMPLES)
        add_test(
            NAME test_standalone_c
            COMMAND ${CMAKE_COMMAND}
                -DCLI_EXE=$<TARGET_FILE:c_tool>
                -DCLI_ARGS=-v
                -DEXPECT_STDOUT=^42$
                -DEXPECT_STDERR=\\[DEBUG\\]
                -P ${TEST_DRIVER}
        )

        add_test(
            NAME test_standalone_cpp
            COMMAND ${CMAKE_COMMAND}
                -DCLI_EXE=$<TARGET_FILE:cpp_tool>
                -DCLI_ARGS=-v
                -DEXPECT_STDOUT=^42$
                -DEXPECT_STDERR=\\[DEBUG\\]
                -P ${TEST_DRIVER}
        )

        if(HAVE_FORTRAN)
            add_test(
                NAME test_standalone_fortran_modern
                COMMAND ${CMAKE_COMMAND}
                    -DCLI_EXE=$<TARGET_FILE:f08_tool>
                    -DCLI_ARGS=-v
                    -DEXPECT_STDOUT=^42$
                    -DEXPECT_STDERR=\\[DEBUG\\]
                    -P ${TEST_DRIVER}
            )

            add_test(
                NAME test_standalone_fortran_legacy
                COMMAND ${CMAKE_COMMAND}
                    -DCLI_EXE=$<TARGET_FILE:f77_tool>
                    -DCLI_ARGS=-v
                    -DEXPECT_STDOUT=^42$
                    -DEXPECT_STDERR=DEBUG
                    -P ${TEST_DRIVER}
            )
        endif()
    endif()
endif()

# ------------------------------------------------------------------------------
# CPack Packaging Configuration
# ------------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "polyglot-cli")
set(CPACK_PACKAGE_VENDOR "FLOSS Project Contributors")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/AltElmer/polyglot-logging")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_STRIP_FILES TRUE)

if(WIN32)
    set(CPACK_GENERATOR "ZIP;TGZ")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;TXZ")
else()
    set(CPACK_GENERATOR "TGZ;TXZ;DEB")
endif()

set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    "/.git/"
    "/build/"
    "/.cache/"
    "\\\\.log$"
)

set(CPACK_DEBIAN_PACKAGE_NAME "polyglot-cli")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Dr. Foad Sojoodi Farimani <foad@example.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

include(CPack)
