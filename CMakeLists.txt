cmake_minimum_required(VERSION 3.28 FATAL_ERROR)
project(modgen)

add_executable(modgen_ast_parser modgen_ast_parser.cpp)
target_compile_features(modgen_ast_parser PRIVATE cxx_std_23)

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} is not supported. Use clang")
endif()

file(REAL_PATH "${CMAKE_CXX_COMPILER}" CLANG_EXECUTABLE_PATH)
cmake_path(GET CLANG_EXECUTABLE_PATH PARENT_PATH CLANG_BIN_PATH)
cmake_path(GET CLANG_BIN_PATH PARENT_PATH CLANG_INSTALL_PREFIX)

set(CLANG_INCLUDE_DIR "${CLANG_INSTALL_PREFIX}/include")
set(CLANG_LIBRARY_PATH "${CLANG_INSTALL_PREFIX}/lib")

find_library(libclang NAMES clang libclang PATHS "${CLANG_LIBRARY_PATH}" REQUIRED)

target_include_directories(modgen_ast_parser PRIVATE "${CLANG_INCLUDE_DIR}")
target_link_directories(modgen_ast_parser PRIVATE "${CLANG_LIBRARY_PATH}")
target_link_libraries(modgen_ast_parser "${libclang}")

include(cmake/modgen.cmake)
include(cmake/std.cmake)

if (PROJECT_IS_TOP_LEVEL)
    add_subdirectory(demo)
endif()