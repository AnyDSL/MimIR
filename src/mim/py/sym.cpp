#include <fe/sym.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace fe {

/*
The current reference policies are placeholders, not everything should be returned as a reference
if python still holds a reference to an object the cpp side has deleted it will lead to UB.
*/

void init_sym(py::module_& m) {
    py::class_<fe::Sym>(m, "Sym")
        .def(py::init<>())
        .def("empty", &fe::Sym::empty, py::return_value_policy::reference)
        .def("size", &fe::Sym::size, py::return_value_policy::reference)
        .def("c_str", &fe::Sym::c_str, py::return_value_policy::reference)
        .def("begin", &fe::Sym::begin, py::return_value_policy::reference)
        .def("cbegin", &fe::Sym::cbegin, py::return_value_policy::reference)
        .def("back", &fe::Sym::back, py::return_value_policy::reference)
        .def("cend", &fe::Sym::cend, py::return_value_policy::reference)
        .def("crend", &fe::Sym::crend, py::return_value_policy::reference)
        .def("front", &fe::Sym::front, py::return_value_policy::reference)
        .def("str", &fe::Sym::str, py::return_value_policy::reference)
        .def("view", &fe::Sym::view, py::return_value_policy::reference)
        .def("rbegin", &fe::Sym::rbegin, py::return_value_policy::reference)
        .def("rend", &fe::Sym::rend, py::return_value_policy::reference);
}

void init_sym_pool(py::module_& m){
    py::class_<fe::SymPool>(m, "SymPool")
        .def(py::init<>())
        .def("sym", static_cast<Sym (SymPool::*)(std::string_view)>(&fe::SymPool::sym))
        .def("sym", static_cast<Sym (SymPool::*)(const std::string&)>(&fe::SymPool::sym))
        .def("sym", static_cast<Sym (SymPool::*)(const char*)>(&fe::SymPool::sym));
}


} // namespace fe