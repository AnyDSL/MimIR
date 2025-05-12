#include <pybind11/pybind11.h>

#include <mim/ast/ast.h>
#include <pybind11/stl.h>


namespace py = pybind11;

namespace mim::ast {

void init_ast(py::module_& m) {
    py::class_<mim::ast::AST>(m, "AST")
        .def(py::init<>())
        //.def(py::init<AST&>())
        .def(py::init<mim::World&>());
        // .def(py::init<>(), static_cast<AST (mim::ast::*)()>(&mim::ast::AST()))
        // .def(py::init<>(), static_cast<AST (mim::ast::*)(mim::World&)>(&mim::ast::AST()))
        // .def(py::init<>(), static_cast<AST (mim::ast::*)(mim::ast::AST&)>(&mim::ast::AST()));
}

} // namespace mim
