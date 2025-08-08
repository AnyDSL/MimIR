#include <fe/sym.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace fe {




void init_sym(py::module_& m) {
    py::class_<fe::Sym>(m, "Sym")
        .def(py::init<>())
        .def("empty", &fe::Sym::empty, py::return_value_policy::reference)
        .def("size", &fe::Sym::size, py::return_value_policy::reference)
        .def("view", &fe::Sym::view, py::return_value_policy::reference)
        .def("str", &fe::Sym::str, py::return_value_policy::reference);
}

void init_sym_pool(py::module_& m){
    py::class_<fe::SymPool>(m, "SymPool")
        .def(py::init<>())
        .def("sym", static_cast<Sym (SymPool::*)(std::string_view)>(&fe::SymPool::sym))
        .def("sym", static_cast<Sym (SymPool::*)(const std::string&)>(&fe::SymPool::sym))
        .def("sym", static_cast<Sym (SymPool::*)(const char*)>(&fe::SymPool::sym));
}


} // namespace fe
