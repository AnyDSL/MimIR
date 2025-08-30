#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <mim/def.h>
#include <mim/world.h>

namespace py = pybind11;

namespace mim {

void init_def(py::module_& m) {
    // clang-format off
    py::class_<mim::Def, std::unique_ptr<mim::Def, py::nodelete>>(m, "Def")
        .def("var",  py::overload_cast<>(&mim::Def::var), py::return_value_policy::reference_internal)
        .def("proj", py::overload_cast<nat_t, nat_t>(&mim::Def::proj, py::const_), py::return_value_policy::reference_internal)
        .def("proj", py::overload_cast<       nat_t>(&mim::Def::proj, py::const_), py::return_value_policy::reference_internal)
        .def("make_external", &mim::Def::make_external)
        .def("dump", py::overload_cast<>(&mim::Def::dump, py::const_), py::return_value_policy::reference_internal);
    // clang-format on
}

} // namespace mim
