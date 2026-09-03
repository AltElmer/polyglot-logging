cmake_minimum_required(VERSION 3.20)
project(StandaloneFortranLegacy LANGUAGES Fortran)

add_executable(f77_tool main.f)

if(CMAKE_Fortran_COMPILER_ID MATCHES "GNU")
    target_compile_options(f77_tool PRIVATE -std=legacy)
endif()
