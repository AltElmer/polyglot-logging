# examples/CMakeLists.txt
add_subdirectory(standalone_c)
add_subdirectory(standalone_cpp)

if(HAVE_FORTRAN)
    add_subdirectory(standalone_fortran_modern)
    add_subdirectory(standalone_fortran_legacy)
endif()
