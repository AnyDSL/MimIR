#include <pybind11/pybind11.h>

#include <mim/driver.h>

#include <pybind11/stl.h>


namespace py = pybind11;

namespace mim {

void init_driver(py::module_& m) {
    py::class_<mim::Driver>(m, "Driver")
        .def(py::init<>())
        .def("world", &mim::Driver::world, py::return_value_policy::reference)
        .def("add_import", &mim::Driver::add_import, py::return_value_policy::reference)
        .def("add_search_path", &mim::Driver::add_search_path, py::return_value_policy::reference);
}

} // namespace mim
