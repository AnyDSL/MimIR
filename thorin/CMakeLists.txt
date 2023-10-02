add_library(libthorin
    axiom.cpp
    axiom.h
    check.cpp
    check.h
    def.cpp
    def.h
    plugin.cpp
    plugin.h
    dump.cpp
    driver.cpp
    driver.h
    lam.cpp
    lam.h
    lattice.cpp
    lattice.h
    rewrite.cpp
    rewrite.h
    tuple.cpp
    tuple.h
    world.cpp
    world.h
    analyses/cfg.cpp
    analyses/cfg.h
    analyses/deptree.cpp
    analyses/deptree.h
    analyses/domfrontier.cpp
    analyses/domfrontier.h
    analyses/domtree.cpp
    analyses/domtree.h
    analyses/looptree.cpp
    analyses/looptree.h
    analyses/schedule.cpp
    analyses/schedule.h
    analyses/scope.cpp
    analyses/scope.h
    be/emitter.h
    be/dot/dot.cpp
    be/dot/dot.h
    be/h/bootstrap.cpp
    be/h/bootstrap.h
    fe/ast.cpp
    fe/ast.h
    fe/lexer.cpp
    fe/lexer.h
    fe/parser.cpp
    fe/parser.h
    fe/scopes.cpp
    fe/scopes.h
    fe/tok.cpp
    fe/tok.h
    pass/optimize.cpp
    pass/pass.cpp
    pass/pipelinebuilder.cpp
    pass/pipelinebuilder.h
    pass/fp/eta_exp.cpp
    pass/fp/eta_exp.h
    pass/fp/eta_red.cpp
    pass/fp/eta_red.h
    pass/fp/beta_red.cpp
    pass/fp/beta_red.h
    pass/fp/tail_rec_elim.cpp
    pass/fp/tail_rec_elim.h
    pass/rw/lam_spec.cpp
    pass/rw/lam_spec.h
    pass/rw/ret_wrap.cpp
    pass/rw/ret_wrap.h
    pass/rw/scalarize.cpp
    pass/rw/scalarize.h
    phase/phase.cpp
    phase/phase.h
    util/array.h
    util/bitset.cpp
    util/bitset.h
    util/dbg.cpp
    util/dbg.h
    util/dl.cpp
    util/dl.h
    util/hash.cpp
    util/hash.h
    util/indexmap.h
    util/indexset.h
    util/log.cpp
    util/log.h
    util/print.cpp
    util/print.h
    util/sys.cpp
    util/sys.h
    util/types.h
    util/utf8.cpp
    util/utf8.h
    util/util.h
)
set_target_properties(libthorin PROPERTIES PREFIX "")
target_compile_features(libthorin PUBLIC cxx_std_${CMAKE_CXX_STANDARD}) # make property public.

if(MSVC AND BUILD_SHARED_LIBS)
    target_compile_definitions(libthorin PUBLIC ABSL_CONSUME_DLL)
endif()

configure_file(
    config.h.in
    "${CMAKE_BINARY_DIR}/include/thorin/config.h"
)
target_include_directories(libthorin
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include/>     # for config.h
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>   # for thorin/*
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../external/half/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(libthorin
    PUBLIC
        absl::btree
        absl::flat_hash_map absl::flat_hash_set
        absl::node_hash_map absl::node_hash_set
        fe rang ${CMAKE_DL_LIBS}
)
install(
    TARGETS libthorin
    EXPORT thorin-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION include FILES_MATCHING PATTERN "*.h"
)
install(
    FILES "${CMAKE_BINARY_DIR}/include/thorin/config.h"
    DESTINATION include/thorin
)
install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/../external/half/include/half.hpp"
    DESTINATION include
)
