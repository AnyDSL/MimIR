#include <pybind11/pybind11.h>

#include <fe/sym.h>

namespace py = pybind11;

namespace nim {

void init_sym(py::module_& m) {
    py::class_<fe::Sym>(m, "Sym")
        .def(py::init<>());
        // .def("empty", &fe::Sym::empty, py::return_value_policy::reference)
        // .def("size", &fe::Sym::size, py::return_value_policy::reference);
}

} // namespace mim