#include <pybind11/pybind11.h>

#include <mim/ast/parser.h>
#include <pybind11/stl.h>


namespace py = pybind11;

namespace mim::ast {

void init_parser(py::module_& m) {
    py::class_<mim::ast::Parser>(m, "Parser")
        .def(py::init<mim::ast::AST>());
        //.def("plugin", static_cast<void (Parser::*)(const std::string&)>, py::return_value_policy::reference);
}

} // namespace mim
