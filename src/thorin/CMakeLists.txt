set(THORIN_SOURCES
    axiom.cpp
    axiom.h
    check.cpp
    check.h
    debug.cpp
    debug.h
    def.cpp
    def.h
    error.cpp
    error.h
    lam.cpp
    lam.h
    lattice.cpp
    lattice.h
    normalize.cpp
    normalize.h
    rewrite.cpp
    rewrite.h
    stream.cpp
    tuple.cpp
    tuple.h
    tables.h
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
    be/ll/ll.cpp
    be/ll/ll.h
    fe/lexer.cpp
    fe/lexer.h
    fe/parser.cpp
    fe/parser.h
    fe/tok.cpp
    fe/tok.h
    pass/optimize.cpp
    pass/pass.cpp
    pass/fp/eta_exp.cpp
    pass/fp/eta_exp.h
    pass/fp/eta_red.cpp
    pass/fp/eta_red.h
    pass/fp/beta_red.cpp
    pass/fp/beta_red.h
    pass/fp/copy_prop.cpp
    pass/fp/copy_prop.h
    pass/fp/dce.cpp
    pass/fp/dce.h
    pass/fp/ssa_constr.cpp
    pass/fp/ssa_constr.h
    pass/fp/tail_rec_elim.cpp
    pass/fp/tail_rec_elim.h
    pass/rw/alloc2malloc.cpp
    pass/rw/alloc2malloc.h
    pass/rw/partial_eval.cpp
    pass/rw/partial_eval.h
    pass/rw/remem_elim.cpp
    pass/rw/remem_elim.h
    pass/rw/ret_wrap.cpp
    pass/rw/ret_wrap.h
    pass/rw/bound_elim.cpp
    pass/rw/bound_elim.h
    pass/rw/scalarize.cpp
    pass/rw/scalarize.h
    transform/cleanup_world.cpp
    transform/cleanup_world.h
    transform/mangle.cpp
    transform/mangle.h
    transform/partial_evaluation.cpp
    transform/partial_evaluation.h
    transform/closure_conv.h
    transform/closure_conv.cpp
    util/array.h
    util/assert.h
    util/bitset.cpp
    util/bitset.h
    util/cast.h
    util/container.h
    util/hash.cpp
    util/hash.h
    util/indexmap.h
    util/indexset.h
    util/ptr.h
    util/stream.cpp
    util/stream.h
    util/types.h
    util/utf8.cpp
    util/utf8.h
)

add_library(libthorin ${THORIN_SOURCES})
set_target_properties(libthorin PROPERTIES PREFIX "")

configure_file(config.h.in config.h.extra)
file(READ ${CMAKE_CURRENT_BINARY_DIR}/config.h.extra THORIN_CONFIG_EXTRA)
generate_export_header(libthorin 
    BASE_NAME thorin 
    EXPORT_MACRO_NAME THORIN_API 
    EXPORT_FILE_NAME config.h
    CUSTOM_CONTENT_FROM_VARIABLE THORIN_CONFIG_EXTRA)

target_include_directories(libthorin PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/..) # for config.h
target_include_directories(libthorin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(libthorin PUBLIC ${PROJECT_SOURCE_DIR}/half/include)
