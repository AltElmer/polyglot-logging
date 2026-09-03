cmake_minimum_required(VERSION 3.20)
project(StandaloneCppExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(cpp_tool main.cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cpp_tool PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(cpp_tool PRIVATE /W4 /permissive- /utf-8)
endif()
