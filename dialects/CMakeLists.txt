add_thorin_dialect(affine
    SOURCES
        affine/affine.cpp
        affine/affine.h
        affine/passes/lower_for.cpp
        affine/passes/lower_for.h
    DEPENDS
        core
    INSTALL
)

add_thorin_dialect(clos
    SOURCES
        clos/clos.cpp
        clos/clos.h
        clos/normalizers.cpp
        clos/pass/fp/lower_typed_clos_prep.cpp
        clos/pass/fp/lower_typed_clos_prep.h
        clos/pass/rw/branch_clos_elim.cpp
        clos/pass/rw/branch_clos_elim.h
        clos/pass/rw/clos2sjlj.cpp
        clos/pass/rw/clos2sjlj.h
        clos/pass/rw/clos_conv_prep.cpp
        clos/pass/rw/clos_conv_prep.h
        clos/phase/clos_conv.cpp
        clos/phase/clos_conv.h
        clos/phase/lower_typed_clos.cpp
        clos/phase/lower_typed_clos.h
    DEPENDS
        mem
    HEADER_DEPENDS
        core
    INSTALL
)

add_thorin_dialect(core
    SOURCES
        core/core.cpp
        core/core.h
        core/normalizers.cpp
        core/be/ll/ll.cpp
        core/be/ll/ll.h
    DEPENDS
        mem
        clos
    INSTALL
)

add_thorin_dialect(direct
    SOURCES
        direct/direct.cpp
        direct/direct.h
        direct/passes/ds2cps.cpp
        direct/passes/ds2cps.h
    INSTALL
)

add_thorin_dialect(mem
    SOURCES
        mem/mem.cpp
        mem/mem.h
        mem/normalizers.cpp
        mem/passes/fp/copy_prop.cpp
        mem/passes/fp/copy_prop.h
        mem/passes/fp/ssa_constr.cpp
        mem/passes/fp/ssa_constr.h
        mem/passes/rw/alloc2malloc.cpp
        mem/passes/rw/alloc2malloc.h
        mem/passes/rw/remem_elim.cpp
        mem/passes/rw/remem_elim.h
    HEADER_DEPENDS
        core
    INSTALL
)

add_thorin_dialect(debug
    SOURCES
        debug/debug.h
        debug/debug.cpp
        debug/passes/remove_perm.h
        debug/passes/remove_perm.cpp
        debug/normalizers.cpp
    INSTALL
)

add_thorin_dialect(direct
    SOURCES
        direct/direct.cpp
        direct/direct.h
        direct/passes/ds2cps.cpp
        direct/passes/ds2cps.h
        direct/normalizers.cpp
    INSTALL
)


# TODO: make this work
# file(GLOB autodiff_SRC
#      "autodiff/passes/*.h"
#      "autodiff/passes/*.cpp"
# )
# ${autodiff_SRC}

add_thorin_dialect(autodiff
    SOURCES
        autodiff/autodiff.cpp
        autodiff/autodiff.h
        autodiff/passes/autodiff_eval.cpp
        autodiff/passes/autodiff_eval.h
        autodiff/passes/autodiff_zero.cpp
        autodiff/passes/autodiff_zero.h
        autodiff/passes/autodiff_zero_cleanup.cpp
        autodiff/passes/autodiff_zero_cleanup.h
        autodiff/auxiliary/autodiff_aux.cpp
        autodiff/auxiliary/autodiff_aux.h
        autodiff/auxiliary/autodiff_rewrite_inner.cpp
        autodiff/auxiliary/autodiff_rewrite_toplevel.cpp
        autodiff/normalizers.cpp
    DEPENDS
        mem
        core
        clos
        affine
        direct
    INSTALL
)
