set(MIM_PLUGINS
    affine
    autodiff
    clos
    core
    demo
    direct
    math
    matrix
    mem
    opt
    ord
    phase
    refly
    regex
    tensor
    tuple
    vec
)

list(TRANSFORM MIM_PLUGINS PREPEND "mim_internal_" OUTPUT_VARIABLE MIM_INTERNAL_PLUGINS)

foreach(PLUGIN ${MIM_PLUGINS})
    add_subdirectory(${PLUGIN})
endforeach()

add_custom_target(mim_all_plugins)
foreach(PLUGIN ${MIM_PLUGINS})
    add_dependencies(mim_all_plugins mim_${PLUGIN})
    add_dependencies(mim_${PLUGIN} ${MIM_INTERNAL_PLUGINS})
endforeach()

add_dependencies(mim_all mim_all_plugins)
