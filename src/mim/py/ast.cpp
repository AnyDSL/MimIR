#include <pybind11/pybind11.h>

#include <mim/ast/ast.h>
#include <pybind11/stl.h>


namespace py = pybind11;

namespace ast {

void init_ast(py::module_& m) {
    py::class_<mim::ast::AST>(m, "AST")
        .def(py::init<>());
}

} // namespace mim
