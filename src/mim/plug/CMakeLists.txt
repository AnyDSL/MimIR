set(THORIN_PLUGINS
    affine
    autodiff
    clos
    compile
    core
    demo
    direct
    math
    matrix
    mem
    opt
    refly
    regex
)

list(TRANSFORM THORIN_PLUGINS PREPEND "thorin_internal_" OUTPUT_VARIABLE THORIN_INTERNAL_PLUGINS)

foreach(PLUGIN ${THORIN_PLUGINS})
    add_subdirectory(${PLUGIN})
endforeach()

add_custom_target(thorin_all_plugins)
foreach(PLUGIN ${THORIN_PLUGINS})
    add_dependencies(thorin_all_plugins thorin_${PLUGIN})
    add_dependencies(thorin_${PLUGIN} ${THORIN_INTERNAL_PLUGINS})
endforeach()

add_dependencies(thorin_all thorin_all_plugins)
