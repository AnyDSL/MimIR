#include <pybind11/pybind11.h>

#include <mim/def.h>
#include <mim/world.h>
#include <pybind11/stl.h>


namespace py = pybind11;

namespace mim {

  void init_def(py::module_& m) {
    py::class_<mim::Def, std::unique_ptr<mim::Def,py::nodelete>>(m, "Def")
    .def("var", static_cast<const mim::Def* (Def::*)()>(&mim::Def::var), py::return_value_policy::reference_internal)
    .def("proj", py::overload_cast<nat_t>(&mim::Def::proj, py::const_), py::return_value_policy::reference_internal)
    .def("make_external", &mim::Def::make_external)
    .def("proj", py::overload_cast<nat_t, nat_t>(&mim::Def::proj, py::const_), py::return_value_policy::reference_internal);

  }

} // namespace mim
