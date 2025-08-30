#include <pybind11/pybind11.h>

#include "parser_wrapper.cpp"
#include <mim/ast/parser.h>
#include <pybind11/stl.h>


namespace py = pybind11;

namespace mim::ast {

void init_parser(py::module_& m) {
    py::class_<mim::ast::Parser, std::unique_ptr<mim::ast::Parser, py::nodelete>>(m, "Parser")
        .def(py::init<mim::ast::AST&>());
        //.def("plugin", static_cast<void (Parser::*)(const std::string&)>, py::return_value_policy::reference);
}

void init_parser_wrapper(py::module_& m){
    py::class_<mim::ast::PyParser>(m, "PyParser")
        .def(py::init<mim::ast::Parser*>())
        .def("plugin", &mim::ast::PyParser::plugin);
}


} // namespace mim::ast