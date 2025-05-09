pybind11_add_module(pymim
    driver.cpp
    world.cpp
    sym.cpp
    py.cpp
)

set_target_properties(pymim PROPERTIES LIBRARY_OUTPUT_NAME mim)
target_link_libraries(pymim PRIVATE libmim)
