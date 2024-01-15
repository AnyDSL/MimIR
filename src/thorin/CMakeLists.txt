add_library(libthorin)
target_sources(libthorin
    PRIVATE
        axiom.cpp
        check.cpp
        def.cpp
        plugin.cpp
        dump.cpp
        driver.cpp
        lam.cpp
        lattice.cpp
        rewrite.cpp
        tuple.cpp
        world.cpp
        analyses/cfg.cpp
        analyses/deptree.cpp
        analyses/domfrontier.cpp
        analyses/domtree.cpp
        analyses/looptree.cpp
        analyses/schedule.cpp
        analyses/scope.cpp
        be/dot/dot.cpp
        be/h/bootstrap.cpp
        fe/ast.cpp
        fe/lexer.cpp
        fe/parser.cpp
        fe/scopes.cpp
        fe/tok.cpp
        pass/beta_red.cpp
        pass/eta_exp.cpp
        pass/eta_red.cpp
        pass/lam_spec.cpp
        pass/optimize.cpp
        pass/pass.cpp
        pass/pipelinebuilder.cpp
        pass/ret_wrap.cpp
        pass/scalarize.cpp
        pass/tail_rec_elim.cpp
        phase/phase.cpp
        util/bitset.cpp
        util/dbg.cpp
        util/dl.cpp
        util/hash.cpp
        util/log.cpp
        util/print.cpp
        util/sys.cpp
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
    "${PROJECT_SOURCE_DIR}/include/thorin/config.h.in"
    "${CMAKE_BINARY_DIR}/include/thorin/config.h"
)
target_include_directories(libthorin
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/external/half/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include> # for config.h
        $<INSTALL_INTERFACE:include>
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
    FILES "${CMAKE_BINARY_DIR}/include/thorin/config.h"
    DESTINATION include/thorin
)
install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/thorin
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

add_subdirectory(cli)
add_subdirectory(plug)

add_dependencies(thorin_all libthorin)
