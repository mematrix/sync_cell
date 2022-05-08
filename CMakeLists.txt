cmake_minimum_required(VERSION 3.10)
project(sync_cell)

set(CMAKE_CXX_STANDARD 20)

message("Build Compiler: ${CMAKE_CXX_COMPILER_ID}")
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(BuildWithMSVC 1)
endif ()

if (BuildWithMSVC)
    add_compile_options("/W3")
    add_compile_options("/WX")
else ()
    add_compile_options("-Wall")
    add_compile_options("-Werror")
    add_compile_options("-march=native")
    add_compile_options("$<$<CONFIG:Release>:-O3>")
endif ()

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
#add_compile_options("$<$<C_COMPILER_ID:MSVC>:/MD>")
#add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/MD>")

include_directories(${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(test)
