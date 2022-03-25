cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(Thorin VERSION 1.9.0)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (default Debug)" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(THORIN_ENABLE_PROFILING "Profile performance of hash functions in thorin::HashTable" ON)
    option(THORIN_ENABLE_CHECKS "Enable expensive checks" ON)
else()
    option(THORIN_ENABLE_PROFILING "Profile performance of hash functions in thorin::HashTable" OFF)
    option(THORIN_ENABLE_CHECKS "Enable expensive checks" OFF)
endif()

if(WIN32)
    option(THORIN_COLOR_TERM "Use colored output (doesn't work in Windows terminal)" FALSE)
    add_compile_definitions(NOMINMAX) # prevents windows.h defining min/max macros
else()
    option(THORIN_COLOR_TERM "Use colored output (doesn't work in Windows terminal)" TRUE)
endif()

if(MSVC)
    set(CMAKE_CXX_STANDARD 23) # /std:c++latest - unlocks non-stable c++20 features
else()
    set(CMAKE_CXX_STANDARD 20) # -std=c++20 for everyone else
endif()

if(BUILD_TESTING)
    include(GoogleTest)
    include(CTest)

    find_program(MEMORYCHECK_COMMAND valgrind)
    set(VALGRIND_COMMAND_OPTIONS "--leak-check=full --error-exitcode=1")

    add_subdirectory(googletest)
endif()

include(GenerateExportHeader)

set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_USE_EXTERNAL_GOOGLETEST ON)
add_subdirectory(abseil-cpp)
add_subdirectory(lyra)

add_subdirectory(src)
add_subdirectory(dialects)

if(BUILD_TESTING)
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
