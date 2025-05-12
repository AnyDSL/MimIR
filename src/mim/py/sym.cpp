#include <fe/sym.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace fe {

/*
The current reference policies are placeholders, not everything should be returned as a reference
if python still holds a reference to an object the cpp side has deleted it will lead to UB.


Operator overloads should be implemented via the dunder methods of a python class, for convenience we might want to implement a few other dunder methods as well,
such as __repr__.
List of dunder methods: https://www.geeksforgeeks.org/dunder-magic-methods-python/
*/

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