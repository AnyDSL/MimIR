#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include <mim/driver.h>
namespace py = pybind11;

namespace mim {

void init_driver(py::module_& m) {
    py::class_<mim::Driver, std::unique_ptr<mim::Driver, py::nodelete>>(m, "Driver")
        .def(py::init<>())
        .def("world", &mim::Driver::world, py::return_value_policy::reference_internal)
        .def("add_import", &mim::Driver::add_import, py::return_value_policy::reference_internal)
        .def("add_search_path", &mim::Driver::add_search_path)
        .def("log", &mim::Driver::log, py::return_value_policy::reference_internal);
}

} // namespace mim
