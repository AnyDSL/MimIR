#---- Cargo integration ----

set(CARGO_WIN_DEBUG_CONFIG ${CMAKE_SOURCE_DIR}/src/mim/plug/eqsat/egg/.cargo/windbg-config.toml)
set(CARGO_MANIFEST ${CMAKE_SOURCE_DIR}/src/mim/plug/eqsat/egg/Cargo.toml)
set(CARGO_TARGET_DIR ${CMAKE_SOURCE_DIR}/build/include/egg)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CARGO_BUILD_TYPE "")
  set(EGG_LIB_DIR debug)
else()
  set(CARGO_BUILD_TYPE --release)
  set(EGG_LIB_DIR release)
endif()

if(WIN32)
  set(CARGO_EXTRA --config ${CARGO_WIN_DEBUG_CONFIG})
endif()

set(EGG_SOURCE_FILE ${CMAKE_SOURCE_DIR}/src/mim/plug/eqsat/egg/src/lib.rs)
set(EGG_BRIDGE_CPP ${CARGO_TARGET_DIR}/cxxbridge/eqsat/src/lib.rs.cc)
set(EGG_LIB ${CARGO_TARGET_DIR}/${EGG_LIB_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}eqsat${CMAKE_STATIC_LIBRARY_SUFFIX})

add_custom_command(
    OUTPUT ${EGG_BRIDGE_CPP} ${EGG_LIB}
    COMMAND cargo build ${CARGO_BUILD_TYPE} ${CARGO_EXTRA} --manifest-path ${CARGO_MANIFEST} --target-dir ${CARGO_TARGET_DIR}
    DEPENDS ${EGG_SOURCE_FILE}
    USES_TERMINAL
    COMMENT "Running cargo..."
)

add_custom_target(egg ALL
    DEPENDS ${EGG_BRIDGE_CPP} ${EGG_LIB}
)

add_library(eqsat STATIC IMPORTED)
# TODO: only required for windows
target_link_libraries(eqsat
    INTERFACE
        ws2_32
        userenv
        advapi32
        ntdll
)
set_target_properties(eqsat PROPERTIES IMPORTED_LOCATION ${EGG_LIB})
add_dependencies(eqsat egg)

#---------------------------

add_library(libmim SHARED)
target_sources(libmim
    PRIVATE
        axm.cpp
        check.cpp
        def.cpp
        dot.cpp
        driver.cpp
        dump.cpp
        lam.cpp
        lattice.cpp
        nest.cpp
        pass.cpp
        phase.cpp
        plugin.cpp
        rewrite.cpp
        rule.cpp
        schedule.cpp
        tuple.cpp
        world.cpp
        ast/ast.cpp
        ast/bind.cpp
        ast/emit.cpp
        ast/lexer.cpp
        ast/parser.cpp
        ast/stream.cpp
        ast/tok.cpp
        pass/beta_red.cpp
        pass/eta_exp.cpp
        pass/eta_red.cpp
        pass/lam_spec.cpp
        pass/optimize.cpp
        pass/scalarize.cpp
        pass/tail_rec_elim.cpp
        phase/beta_red_phase.cpp
        phase/eta_red_phase.cpp
        phase/eta_exp_phase.cpp
        phase/branch_normalize.cpp
        phase/prefix_cleanup.cpp
        phase/ret_wrap.cpp
        phase/sccp.cpp
        util/dbg.cpp
        util/dl.cpp
        util/log.cpp
        util/print.cpp
        util/sys.cpp
)

set_target_properties(libmim PROPERTIES PREFIX "")
target_compile_features(libmim PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(libmim PUBLIC /utf-8 /wd4146 /wd4245)
  if(BUILD_SHARED_LIBS)
    target_compile_definitions(libmim PUBLIC ABSL_CONSUME_DLL)
  endif()
endif()

configure_file(
    "${PROJECT_SOURCE_DIR}/include/mim/config.h.in"
    "${CMAKE_BINARY_DIR}/include/mim/config.h"
)
target_include_directories(libmim
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/external/half/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include> # for config.h
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(libmim
    PUBLIC
        eqsat
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
    FILES "${CMAKE_BINARY_DIR}/include/mim/config.h"
    DESTINATION include/mim
)
install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/mim
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

add_subdirectory(cli)
add_subdirectory(plug)

add_dependencies(mim_all libmim)
