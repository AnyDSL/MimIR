#include <pybind11/pybind11.h>

#include <mim/world.h>

namespace py = pybind11;

namespace mim {

void init_world(py::module_& m) {
    // clang-format off
    py::class_<mim::World>(m, "World")
        .def("write", static_cast<void (World::*)()>(&mim::World::write));
    // clang-format on
}

} // namespace mim
