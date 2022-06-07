set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/thorin)

add_library(thorin_foo MODULE
    foo.cpp
    foo.h
)
set_target_properties(thorin_foo
    PROPERTIES 
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN 1
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
target_link_libraries(thorin_foo libthorin)

add_thorin_dialect(core)

add_library(thorin_core MODULE
    core/core.cpp
    core/core.h
    core/normalizers.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/core.h
)

set_target_properties(thorin_core
    PROPERTIES 
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN 1
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
target_link_libraries(thorin_core libthorin)

add_thorin_dialect(mem)

add_library(thorin_mem MODULE
    mem/passes/fp/copy_prop.cpp
    mem/passes/fp/copy_prop.h
    mem/passes/fp/ssa_constr.cpp
    mem/passes/fp/ssa_constr.h
    mem/passes/rw/alloc2malloc.cpp
    mem/passes/rw/alloc2malloc.h
    mem/passes/rw/remem_elim.cpp
    mem/passes/rw/remem_elim.h
    mem/be/ll/ll.cpp
    mem/be/ll/ll.h
    mem/mem.cpp
    mem/mem.h
    mem/normalizers.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/mem.h
    ${CMAKE_CURRENT_BINARY_DIR}/core.h
)

set_target_properties(thorin_mem
    PROPERTIES 
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN 1
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
target_link_libraries(thorin_mem libthorin)

add_thorin_dialect(affine)

add_library(thorin_affine MODULE
    affine/affine.cpp
    affine/affine.h
    ${CMAKE_CURRENT_BINARY_DIR}/affine.h
    affine/passes/lower_for.cpp
    affine/passes/lower_for.h
)

set_target_properties(thorin_affine
    PROPERTIES 
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN 1
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
target_link_libraries(thorin_affine libthorin)
add_dependencies(thorin_affine thorin_core thorin_mem)
