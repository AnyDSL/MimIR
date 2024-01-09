cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(Thorin VERSION 1.9.0)

include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(CMakePrintHelpers)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(Thorin)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(THORIN_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/thorin)

option(BUILD_SHARED_LIBS           "Build shared libraries." ON)
option(THORIN_BUILD_DOCS           "If ON, Thorin will build the documentation (requires Doxygen)." OFF)
option(THORIN_BUILD_EXAMPLES       "If ON, Thorin will build examples." OFF)
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

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    find_program(MEMORYCHECK_COMMAND valgrind)
    set(VALGRIND_COMMAND_OPTIONS "--leak-check=full --error-exitcode=1")
    option(BUILD_TESTING "Build lit and unit tests." OFF) # needs to be BEFORE "include(CTest)"
    include(CTest)
    cmake_dependent_option(THORIN_LIT_WITH_VALGRIND "If ON, the Thorin CLI in the lit tests will be run under valgrind." OFF "BUILD_TESTING" OFF)
    if(BUILD_TESTING)
        include(GoogleTest)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        add_subdirectory(external/googletest EXCLUDE_FROM_ALL)
        add_subdirectory(gtest)
        add_subdirectory(lit)
    endif()
endif()

set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_USE_EXTERNAL_GOOGLETEST ON)
set(ABSL_ENABLE_INSTALL ON)
set(FE_ABSL ON)
set(FE_INSTALL_DEPENDENCIES ${THORIN_INSTALL_DEPENDENCIES})
if(NOT THORIN_INSTALL_DEPENDENCIES)
    add_subdirectory(external/abseil-cpp EXCLUDE_FROM_ALL)
    add_subdirectory(external/fe         EXCLUDE_FROM_ALL)
    add_subdirectory(external/lyra       EXCLUDE_FROM_ALL)
    add_subdirectory(external/rang       EXCLUDE_FROM_ALL)
else()
    add_subdirectory(external/abseil-cpp)
    add_subdirectory(external/fe)
    add_subdirectory(external/lyra)
    add_subdirectory(external/rang)
endif()

add_library(libthorin)
target_sources(libthorin
    PRIVATE
        src/thorin/axiom.cpp
        src/thorin/check.cpp
        src/thorin/def.cpp
        src/thorin/plugin.cpp
        src/thorin/dump.cpp
        src/thorin/driver.cpp
        src/thorin/lam.cpp
        src/thorin/lattice.cpp
        src/thorin/rewrite.cpp
        src/thorin/tuple.cpp
        src/thorin/world.cpp
        src/thorin/analyses/cfg.cpp
        src/thorin/analyses/deptree.cpp
        src/thorin/analyses/domfrontier.cpp
        src/thorin/analyses/domtree.cpp
        src/thorin/analyses/looptree.cpp
        src/thorin/analyses/schedule.cpp
        src/thorin/analyses/scope.cpp
        src/thorin/be/dot/dot.cpp
        src/thorin/be/h/bootstrap.cpp
        src/thorin/fe/ast.cpp
        src/thorin/fe/lexer.cpp
        src/thorin/fe/parser.cpp
        src/thorin/fe/scopes.cpp
        src/thorin/fe/tok.cpp
        src/thorin/pass/beta_red.cpp
        src/thorin/pass/eta_exp.cpp
        src/thorin/pass/eta_red.cpp
        src/thorin/pass/lam_spec.cpp
        src/thorin/pass/optimize.cpp
        src/thorin/pass/pass.cpp
        src/thorin/pass/pipelinebuilder.cpp
        src/thorin/pass/ret_wrap.cpp
        src/thorin/pass/scalarize.cpp
        src/thorin/pass/tail_rec_elim.cpp
        src/thorin/phase/phase.cpp
        src/thorin/util/bitset.cpp
        src/thorin/util/dbg.cpp
        src/thorin/util/dl.cpp
        src/thorin/util/hash.cpp
        src/thorin/util/log.cpp
        src/thorin/util/print.cpp
        src/thorin/util/sys.cpp
    PUBLIC
        FILE_SET libthorin_headers
        BASE_DIRS include
        TYPE HEADERS
        FILES
            include/thorin/axiom.h
            include/thorin/check.h
            include/thorin/def.h
            include/thorin/plugin.h
            include/thorin/driver.h
            include/thorin/lam.h
            include/thorin/lattice.h
            include/thorin/rewrite.h
            include/thorin/tuple.h
            include/thorin/world.h
            include/thorin/analyses/cfg.h
            include/thorin/analyses/deptree.h
            include/thorin/analyses/domfrontier.h
            include/thorin/analyses/domtree.h
            include/thorin/analyses/looptree.h
            include/thorin/analyses/schedule.h
            include/thorin/analyses/scope.h
            include/thorin/be/dot/dot.h
            include/thorin/be/emitter.h
            include/thorin/be/h/bootstrap.h
            include/thorin/fe/ast.h
            include/thorin/fe/lexer.h
            include/thorin/fe/parser.h
            include/thorin/fe/scopes.h
            include/thorin/fe/tok.h
            include/thorin/pass/beta_red.h
            include/thorin/pass/eta_exp.h
            include/thorin/pass/eta_red.h
            include/thorin/pass/lam_spec.h
            include/thorin/pass/pipelinebuilder.h
            include/thorin/pass/ret_wrap.h
            include/thorin/pass/scalarize.h
            include/thorin/pass/tail_rec_elim.h
            include/thorin/phase/phase.h
            include/thorin/util/bitset.h
            include/thorin/util/dbg.h
            include/thorin/util/dl.h
            include/thorin/util/hash.h
            include/thorin/util/indexmap.h
            include/thorin/util/indexset.h
            include/thorin/util/log.h
            include/thorin/util/print.h
            include/thorin/util/span.h
            include/thorin/util/sys.h
            include/thorin/util/types.h
            include/thorin/util/util.h
            include/thorin/util/vector.h
)

set_target_properties(libthorin PROPERTIES PREFIX "")

if(MSVC)
    target_compile_features(libthorin PUBLIC cxx_std_23) # /std:c++latest - unlocks non-stable c++20 features
    target_compile_options(libthorin PUBLIC /utf-8 /wd4146 /wd4245)
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(libthorin PUBLIC ABSL_CONSUME_DLL)
    endif()
else()
    target_compile_features(libthorin PUBLIC cxx_std_20)
endif()

configure_file(
    include/thorin/config.h.in
    "${CMAKE_BINARY_DIR}/include/thorin/config.h"
)
target_include_directories(libthorin
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include> # for config.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/half/include>
)
target_link_libraries(libthorin
    PUBLIC
        absl::btree
        absl::fixed_array
        absl::inlined_vector
        absl::flat_hash_map absl::flat_hash_set
        absl::node_hash_map absl::node_hash_set
        fe
        rang
        ${CMAKE_DL_LIBS}
)
install(
    TARGETS libthorin
    EXPORT thorin-targets
    FILE_SET libthorin_headers
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    FILES "${CMAKE_BINARY_DIR}/include/thorin/config.h"
    DESTINATION include/thorin
)
install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/../external/half/include/half.hpp"
    DESTINATION include
)

add_subdirectory(cli)
add_subdirectory(libs)
add_subdirectory(plugins)

if(THORIN_BUILD_EXAMPLES)
    add_subdirectory(examples)
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
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/thorin-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/thorin-config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/thorin"
)
install(
    EXPORT thorin-targets
    NAMESPACE thorin::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/thorin"
)
install(
    FILES
        cmake/Thorin.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/thorin-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/thorin-config-version.cmake"
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/thorin"
)
