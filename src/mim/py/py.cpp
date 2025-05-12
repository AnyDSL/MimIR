#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace mim {
void init_world(py::module_&);
void init_driver(py::module_&);
void init_sym(py::module_&);
} // namespace mim

namespace fe{
void init_sym(py::module_&);
void init_sym_pool(py::module_&);
}
PYBIND11_MODULE(mim, m) {
    mim::init_driver(m);
    mim::init_world(m);
    fe::init_sym(m);
    fe::init_sym_pool(m);
}
