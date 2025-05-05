#include <pybind11/pybind11.h>

#include <mim/world.h>

namespace py = pybind11;

void init_world(py::module_& m) { py::class_<mim::World>(m, "World"); }
