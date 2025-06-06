add_library(libmim)
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
        normalize.cpp
        plugin.cpp
        rewrite.cpp
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
        pass/pass.cpp
        pass/pipelinebuilder.cpp
        pass/ret_wrap.cpp
        pass/scalarize.cpp
        pass/tail_rec_elim.cpp
        phase/phase.cpp
        util/bitset.cpp
        util/dbg.cpp
        util/dl.cpp
        util/log.cpp
        util/print.cpp
        util/sys.cpp
)

set_target_properties(libmim PROPERTIES PREFIX "")

if(MSVC)
    target_compile_features(libmim PUBLIC cxx_std_23) # /std:c++latest - unlocks non-stable c++20 features
    target_compile_options(libmim PUBLIC /utf-8 /wd4146 /wd4245)
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(libmim PUBLIC ABSL_CONSUME_DLL)
    endif()
else()
    target_compile_features(libmim PUBLIC cxx_std_20)
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
