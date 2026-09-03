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

# ------------------------------------------------------------------------------
# Dependency Management: spdlog via FetchContent
# ------------------------------------------------------------------------------
include(FetchContent)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE INTERNAL "Disable spdlog examples")
set(SPDLOG_BUILD_TESTS OFF CACHE INTERNAL "Disable spdlog tests")
set(SPDLOG_INSTALL OFF CACHE INTERNAL "Disable spdlog install rules")
set(SPDLOG_BUILD_SHARED OFF CACHE INTERNAL "Build static spdlog")

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

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
    PUBLIC
        spdlog::spdlog
)

# ------------------------------------------------------------------------------
# Target 2: C Subsystem (Pure C numerical component)
# ------------------------------------------------------------------------------
add_library(c_subsystem STATIC
    src/c_subsystem.c
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
    )

    target_link_libraries(polyglot_logger_fortran
        PUBLIC
            polyglot_logger_core
    )
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
endif()

if(HAVE_FORTRAN)
    if(CMAKE_Fortran_COMPILER_ID MATCHES "GNU")
        target_compile_options(polyglot_logger_fortran PRIVATE
            -Wall -Wextra -Wimplicit-interface -fimplicit-none
        )
    elseif(CMAKE_Fortran_COMPILER_ID MATCHES "Intel|IntelLLVM")
        target_compile_options(polyglot_logger_fortran PRIVATE
            /warn:all /stand:f08
        )
    endif()
endif()

# ------------------------------------------------------------------------------
# Installation Rules (Prerequisite for CPack)
# ------------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS cli_tool
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Application
)

install(FILES include/polyglot_log.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
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

    # Test 1: STDOUT Isolation (Application data must be clean with zero log leakage)
    add_test(
        NAME test_stdout_data_cleanliness
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DEXPECT_STDOUT=^COMPUTATION_SUCCESS: 42$
            -DFORBID_STDOUT=(\\[info\\]|\\[debug\\]|\\[trace\\])
            -P ${TEST_DRIVER}
    )

    # Test 2: Default Log Level (stderr must contain [info], but NOT [debug] or [trace])
    add_test(
        NAME test_default_verbosity_filtering
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DEXPECT_STDERR=\\[info\\]
            -DFORBID_STDERR=(\\[debug\\]|\\[trace\\])
            -P ${TEST_DRIVER}
    )

    # Test 3: Verbose Flag (stderr must show [debug] output when -v is passed)
    add_test(
        NAME test_verbose_debug_flag
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DCLI_ARGS=-v
            -DEXPECT_STDERR=\\[debug\\]
            -DFORBID_STDERR=\\[trace\\]
            -P ${TEST_DRIVER}
    )

    # Test 4: Trace Flag (stderr must show [trace] output when -vv is passed)
    add_test(
        NAME test_trace_verbosity_flag
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DCLI_ARGS=-vv
            -DEXPECT_STDERR=\\[trace\\]
            -P ${TEST_DRIVER}
    )

    # Test 5: Quiet Flag (stderr must suppress [info] and [debug])
    add_test(
        NAME test_quiet_mode_flag
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DCLI_ARGS=-q
            -DFORBID_STDERR=(\\[info\\]|\\[debug\\]|\\[trace\\])
            -P ${TEST_DRIVER}
    )

    # Test 6: Dual-Sink File Logging (Verify disk write, ISO timestamp, thread info)
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

    # Test 7: Help Flag (Emits usage and exits 0)
    add_test(
        NAME test_help_flag
        COMMAND ${CMAKE_COMMAND}
            -DCLI_EXE=$<TARGET_FILE:cli_tool>
            -DCLI_ARGS=--help
            -DEXPECT_STDOUT=Usage:.*--help
            -P ${TEST_DRIVER}
    )

    # Test 8: Polyglot Subsystem Logging (C-Subsystem and Fortran-Solver logs present)
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
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/example/polyglot-cli")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_STRIP_FILES TRUE)

# Select appropriate package generators based on platform
if(WIN32)
    set(CPACK_GENERATOR "ZIP;TGZ")
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

# Debian (.deb) specific metadata
set(CPACK_DEBIAN_PACKAGE_NAME "polyglot-cli")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Dr. Foad Sojoodi Farimani <foad@example.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

include(CPack)
