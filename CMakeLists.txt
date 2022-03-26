cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(Thorin VERSION 1.9.0)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (default Debug)" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(MSVC)
    set(CMAKE_CXX_STANDARD 23) # /std:c++latest - unlocks non-stable c++20 features
    add_compile_options(/utf-8 /wd4146 /wd4245)
else()
    set(CMAKE_CXX_STANDARD 20) # -std=c++20 for everyone else
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(THORIN_ENABLE_CHECKS "Enable expensive checks" ON)
else()
    option(THORIN_ENABLE_CHECKS "Enable expensive checks" OFF)
endif()

if(WIN32)
    option(THORIN_COLOR_TERM "Use colored output (doesn't work in Windows terminal)" FALSE)
    add_compile_definitions(NOMINMAX) # prevents windows.h defining min/max macros
else()
    option(THORIN_COLOR_TERM "Use colored output (doesn't work in Windows terminal)" TRUE)
endif()

include(CTest)
if(BUILD_TESTING)
    include(GoogleTest)
    add_subdirectory(modules/googletest)

    find_program(MEMORYCHECK_COMMAND valgrind)
    set(VALGRIND_COMMAND_OPTIONS "--leak-check=full --error-exitcode=1")
endif()

set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_USE_EXTERNAL_GOOGLETEST ON)
add_subdirectory(modules/abseil-cpp)
add_subdirectory(modules/lyra)

add_subdirectory(thorin)
add_subdirectory(cli)
add_subdirectory(dialects)

if(BUILD_TESTING)
    add_subdirectory(gtest)
endif()
