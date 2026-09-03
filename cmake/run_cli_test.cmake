# cmake/run_cli_test.cmake
# Generic cross-platform test driver script for CTest
cmake_minimum_required(VERSION 3.24)

if(NOT EXISTS "${CLI_EXE}")
    message(FATAL_ERROR "Target executable not found: '${CLI_EXE}'")
endif()

# Clean up any leftover file from previous runs
if(EXPECT_FILE AND EXISTS "${EXPECT_FILE}")
    file(REMOVE "${EXPECT_FILE}")
endif()

# Parse CLI arguments into a list
if(DEFINED CLI_ARGS)
    list(LENGTH CLI_ARGS CLI_ARGS_LEN)
    if(CLI_ARGS_LEN GREATER 1)
        set(ARG_LIST ${CLI_ARGS})
    else()
        separate_arguments(ARG_LIST NATIVE_COMMAND "${CLI_ARGS}")
    endif()
else()
    set(ARG_LIST "")
endif()

# Set environment variables for the child process
if(TEST_ENV)
    string(REGEX MATCH "^([^=]+)=(.*)$" _match "${TEST_ENV}")
    if(_match)
        set(ENV{${CMAKE_MATCH_1}} "${CMAKE_MATCH_2}")
    endif()
endif()

# Execute CLI binary directly via process API (no shell interception)
execute_process(
    COMMAND "${CLI_EXE}" ${ARG_LIST}
    OUTPUT_VARIABLE ACTUAL_STDOUT
    ERROR_VARIABLE ACTUAL_STDERR
    RESULT_VARIABLE ACTUAL_EXIT_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

# 1. Assert exit code
set(TARGET_EXIT_CODE 0)
if(DEFINED EXPECT_EXIT_CODE)
    set(TARGET_EXIT_CODE ${EXPECT_EXIT_CODE})
endif()

if(NOT ACTUAL_EXIT_CODE EQUAL TARGET_EXIT_CODE)
    message(FATAL_ERROR "Process exited with code ${ACTUAL_EXIT_CODE} (expected ${TARGET_EXIT_CODE}).\nSTDERR:\n${ACTUAL_STDERR}\nSTDOUT:\n${ACTUAL_STDOUT}")
endif()

# 2. Assert STDOUT rules (Data Payload)
if(EXPECT_STDOUT AND NOT ACTUAL_STDOUT MATCHES "${EXPECT_STDOUT}")
    message(FATAL_ERROR "STDOUT did not match expected pattern '${EXPECT_STDOUT}'.\nActual STDOUT:\n'${ACTUAL_STDOUT}'")
endif()

if(FORBID_STDOUT AND ACTUAL_STDOUT MATCHES "${FORBID_STDOUT}")
    message(FATAL_ERROR "STDOUT leaked forbidden diagnostic output matching '${FORBID_STDOUT}'.\nActual STDOUT:\n'${ACTUAL_STDOUT}'")
endif()

# 3. Assert STDERR rules (Diagnostic Logs)
if(EXPECT_STDERR AND NOT ACTUAL_STDERR MATCHES "${EXPECT_STDERR}")
    message(FATAL_ERROR "STDERR did not match expected pattern '${EXPECT_STDERR}'.\nActual STDERR:\n'${ACTUAL_STDERR}'")
endif()

if(FORBID_STDERR AND ACTUAL_STDERR MATCHES "${FORBID_STDERR}")
    message(FATAL_ERROR "STDERR contained forbidden pattern matching '${FORBID_STDERR}'.\nActual STDERR:\n'${ACTUAL_STDERR}'")
endif()

# 4. Assert File Sink rules
if(EXPECT_FILE)
    if(NOT EXISTS "${EXPECT_FILE}")
        message(FATAL_ERROR "Expected log file was not generated: ${EXPECT_FILE}")
    endif()

    if(EXPECT_FILE_REGEX)
        file(READ "${EXPECT_FILE}" FILE_CONTENT)
        if(NOT FILE_CONTENT MATCHES "${EXPECT_FILE_REGEX}")
            message(FATAL_ERROR "Log file content did not match '${EXPECT_FILE_REGEX}'.\nFile Content:\n${FILE_CONTENT}")
        endif()
    endif()

    # Clean up after successful assertion
    file(REMOVE "${EXPECT_FILE}")
endif()
