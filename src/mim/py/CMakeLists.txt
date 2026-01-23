pybind11_add_module(_mim_core
    bindings/world_wrapper.cpp
    bindings/driver.cpp
    bindings/world.cpp
    bindings/sym.cpp
    bindings/py.cpp
    bindings/ast.cpp
    bindings/# app.cpp
    bindings/log.cpp
    bindings/flags.cpp
    bindings/lam.cpp
    bindings/parser.cpp
    bindings/parser_wrapper.cpp
#   bindings/ def_wrapper.cpp
    bindings/def.cpp
    bindings/error.cpp
)

set_target_properties(_mim_core PROPERTIES LIBRARY_OUTPUT_NAME _mim_core)
target_link_libraries(_mim_core PRIVATE libmim)
