cmake_minimum_required(VERSION 3.20)
project(StandaloneCExample LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(c_tool main.c)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(c_tool PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(c_tool PRIVATE /W4)
endif()
