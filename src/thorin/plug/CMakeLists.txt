add_custom_target(thorin_all_plugins) # must be registered before `add_thorin_plugin` calls

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

include(Thorin) # include *after* THORIN_INTERNAL_PLUGINS has been defined

foreach(PLUGIN ${THORIN_PLUGINS})
    add_subdirectory(${PLUGIN})
endforeach()
