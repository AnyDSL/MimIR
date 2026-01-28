pybind11_add_module(mim
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
    parser_wrapper.cpp
#    def_wrapper.cpp
    def.cpp
    error.cpp
)

target_link_libraries(mim PRIVATE libmim)
