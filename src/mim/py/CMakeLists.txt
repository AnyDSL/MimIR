# find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

pybind11_add_module(pymim
    driver.cpp
    world.cpp
    py.cpp
)

target_link_libraries(pymim PRIVATE libmim)
