# TODO can we get rid of this line?
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/thorin)

add_thorin_dialect(mem
    SOURCES
        mem/passes/fp/copy_prop.cpp
        mem/passes/fp/copy_prop.h
        mem/passes/fp/ssa_constr.cpp
        mem/passes/fp/ssa_constr.h
        mem/passes/rw/alloc2malloc.cpp
        mem/passes/rw/alloc2malloc.h
        mem/passes/rw/remem_elim.cpp
        mem/passes/rw/remem_elim.h
        mem/mem.cpp
        mem/mem.h
        mem/normalizers.cpp
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
)

add_thorin_dialect(affine
    SOURCES
        affine/affine.cpp
        affine/affine.h
        affine/passes/lower_for.cpp
        affine/passes/lower_for.h
    DEPENDS
        core
        mem
)
