#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace mim {
void init_world(py::module_&);
void init_driver(py::module_&);
void init_sym(py::module_&);
void init_flags(py::module_&);
void init_log(py::module_&);
} // namespace mim

namespace mim::ast{
    void init_ast(py::module_&);
    //void init_parser(py::module_&);
}


namespace fe{
void init_sym(py::module_&);
void init_sym_pool(py::module_&);
}

PYBIND11_MODULE(mim, m) {
    mim::init_driver(m);
    mim::init_world(m);
    fe::init_sym(m);
    fe::init_sym_pool(m);
    mim::ast::init_ast(m);
    mim::init_log(m);
    mim::init_flags(m);
}
