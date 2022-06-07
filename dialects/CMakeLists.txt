set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib/thorin)

add_thorin_dialect(core)
add_thorin_dialect(mem)
add_thorin_dialect(affine)

# TODO move this stuff into the add_thorin_dialect function
add_library(thorin_affine MODULE
    affine/affine.cpp
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
