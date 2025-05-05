#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace mim {
void init_world(py::module_&);
void init_driver(py::module_&);
} // namespace mim

PYBIND11_MODULE(mim, m) {
    mim::init_driver(m);
    mim::init_world(m);
}
