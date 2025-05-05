#include <pybind11/pybind11.h>

#include <mim/driver.h>

namespace py = pybind11;

void init_driver(py::module_& m) {
    py::class_<mim::Driver>(m, "Driver").def(py::init<>()).def("world", &mim::Driver::world);
}
