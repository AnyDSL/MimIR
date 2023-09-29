cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(Thorin VERSION 1.9.0)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(Thorin)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(MSVC)
    set(CMAKE_CXX_STANDARD 23) # /std:c++latest - unlocks non-stable c++20 features
    add_compile_options(/utf-8 /wd4146 /wd4245)
else()
    set(CMAKE_CXX_STANDARD 20) # -std=c++20 for everyone else
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(BUILD_SHARED_LIBS           "Build shared libraries." ON)
option(THORIN_BUILD_DOCS           "If ON, Thorin will build the documentation (requires Doxygen)." OFF)
option(THORIN_BUILD_EXAMPLES       "If ON, Thorin will build examples." OFF)
option(THORIN_BUILD_TESTING        "If ON, Thorin will build all of Thorin's own tests." OFF)
option(THORIN_LIT_WITH_VALGRIND    "If ON, the Thorin CLI in the lit tests will be run under valgrind." OFF)
option(THORIN_INSTALL_DEPENDENCIES "If ON, Thorin's dependencies will be installed alongside Thorin (use when not installing globally)." OFF)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}; shared libs: ${BUILD_SHARED_LIBS}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(THORIN_ENABLE_CHECKS "Enable expensive checks" ON)
else()
    option(THORIN_ENABLE_CHECKS "Enable expensive checks" OFF)
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX) # prevents windows.h defining min/max macros
else()
    add_compile_options(-Wall -Wextra)
endif()

set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_USE_EXTERNAL_GOOGLETEST ON)
set(ABSL_ENABLE_INSTALL ON)
if(NOT THORIN_INSTALL_DEPENDENCIES)
    add_subdirectory(modules/abseil-cpp EXCLUDE_FROM_ALL)
    add_subdirectory(modules/rang       EXCLUDE_FROM_ALL)
    add_subdirectory(modules/lyra       EXCLUDE_FROM_ALL)
else()
    add_subdirectory(modules/abseil-cpp)
    add_subdirectory(modules/rang)
    add_subdirectory(modules/lyra)
endif()
add_subdirectory(thorin)
add_subdirectory(cli)
add_subdirectory(dialects)

if(THORIN_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR THORIN_BUILD_TESTING) AND BUILD_TESTING)
    find_program(MEMORYCHECK_COMMAND valgrind)
    set(VALGRIND_COMMAND_OPTIONS "--leak-check=full --error-exitcode=1") # needs to be BEFORE "include(CTest)"
    include(CTest)

    include(GoogleTest)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(modules/googletest EXCLUDE_FROM_ALL)

    add_subdirectory(gtest)
    add_subdirectory(lit)
endif()

if(THORIN_BUILD_DOCS)
    find_package(Doxygen REQUIRED dot)
    if(DOXYGEN_FOUND)
        add_subdirectory(docs)
    endif()
endif()

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/thorin-config-version.cmake"
    VERSION ${Thorin_VERSION}
    COMPATIBILITY SameMajorVersion)

set(THORIN_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/thorin")

install(
    FILES cmake/Thorin.cmake
    DESTINATION ${THORIN_CMAKE_INSTALL_DIR})

install(
    EXPORT thorin_exports
    FILE thorin-targets.cmake
    NAMESPACE thorin::
    DESTINATION ${THORIN_CMAKE_INSTALL_DIR})

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/thorin-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/thorin-config.cmake"
    INSTALL_DESTINATION ${THORIN_CMAKE_INSTALL_DIR})

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/thorin-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/thorin-config-version.cmake"
    DESTINATION "${THORIN_CMAKE_INSTALL_DIR}")
