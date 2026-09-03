cmake_minimum_required(VERSION 3.20)
project(StandaloneFortranModern LANGUAGES Fortran)

set(CMAKE_Fortran_STANDARD 2008)
set(CMAKE_Fortran_STANDARD_REQUIRED ON)

add_executable(f08_tool main.f90)

if(CMAKE_Fortran_COMPILER_ID MATCHES "GNU")
    target_compile_options(f08_tool PRIVATE -Wall -Wextra -fimplicit-none)
elseif(CMAKE_Fortran_COMPILER_ID MATCHES "Intel|IntelLLVM")
    target_compile_options(f08_tool PRIVATE /warn:all /stand:f08)
endif()
