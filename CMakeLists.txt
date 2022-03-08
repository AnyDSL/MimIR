cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(Thorin VERSION 1.9.0)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "limited config" FORCE)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "limited config" FORCE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(THORIN_ENABLE_PROFILING "profile complexity in thorin::HashTable - only works in Debug build" ON)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

include(GenerateExportHeader)
include(CTest)

find_program(MEMORYCHECK_COMMAND valgrind)
set(VALGRIND_COMMAND_OPTIONS "--leak-check=full --error-exitcode=1")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(THORIN_ENABLE_CHECKS TRUE)
endif()

if(WIN32)
    set(THORIN_COLOR_TERM FALSE) # escape sequences not supported here
else()
    set(THORIN_COLOR_TERM TRUE)
endif()

if(MSVC)
    set(CMAKE_CXX_STANDARD 23) # /std:c++latest - unlocks non-stable c++20 features
    add_compile_options(/utf-8 /W4 /wd4146 /wd4245) # /WX
else()
    set(CMAKE_CXX_STANDARD 20) # -std=c++20 for everyone else
    add_compile_options(-Wall -Wextra -pedantic) # -Werror
endif()

add_subdirectory(src)
add_subdirectory(dialects)

if(BUILD_TESTING)
    include(GoogleTest)
    add_subdirectory(googletest)
    add_subdirectory(gtest)

    # keep cache clean
    mark_as_advanced(
        BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
        gmock_build_tests gtest_build_samples gtest_build_tests
        gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols
    )

    # keep IDEs folders clean
    set_target_properties(gtest      PROPERTIES FOLDER extern)
    set_target_properties(gtest_main PROPERTIES FOLDER extern)
    set_target_properties(gmock      PROPERTIES FOLDER extern)
    set_target_properties(gmock_main PROPERTIES FOLDER extern)
endif()
