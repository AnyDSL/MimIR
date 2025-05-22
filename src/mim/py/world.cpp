#include <pybind11/pybind11.h>

#include "world_wrapper.cpp"
#include <mim/world.h>
#include <fe/sym.h>
#include <mim/def.h>
namespace py = pybind11;

namespace mim {

void init_world_wrapper(py::module_& m) {
    // clang-format off
    py::class_<mim::PyWorld>(m, "PyWorld")
        .def(py::init<World*>())
        .def("write", &mim::PyWorld::write, py::return_value_policy::reference)
        .def("annex", &mim::PyWorld::annex)
        .def("implicit_app", &mim::PyWorld::implicit_app, py::return_value_policy::reference)
        .def("type_i32", &mim::PyWorld::type_i32, py::return_value_policy::reference);

    // clang-format on
}

void init_world(py::module_& m) {
    py::class_<mim::World>(m, "World");
// -        .def("write", static_cast<void (World::*)()>(&mim::World::write));
}
} // namespace mim
