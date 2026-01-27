pybind11_add_module(_mim_core
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

set_target_properties(_mim_core PROPERTIES LIBRARY_OUTPUT_NAME _mim_core)
set_target_properties(_mim_core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/src/mim/py/mim"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/src/mim/py/mim"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/src/mim/py/mim"
)
target_link_libraries(_mim_core PRIVATE libmim)
