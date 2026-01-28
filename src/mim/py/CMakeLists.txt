pybind11_add_module(pymim
    world_wrapper.cpp
    driver.cpp
    world.cpp
    sym.cpp
    py.cpp
    ast.cpp
    # app.cpp
    log.cpp
    flags.cpp
    lam.cpp
    parser.cpp
    world_wrapper.cpp
    parser_wrapper.cpp
#    def_wrapper.cpp
    def.cpp
    error.cpp
)

set_target_properties(pymim PROPERTIES LIBRARY_OUTPUT_NAME mim)
target_link_libraries(pymim PRIVATE libmim)