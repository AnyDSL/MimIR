#include <fe/sym.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <mim/def.h>
#include <mim/world.h>

#include "world_wrapper.cpp"
namespace py = pybind11;

namespace mim {

void init_world_wrapper(py::module_& m) {
    // clang-format off
    py::class_<mim::PyWorld>(m, "PyWorld")
        .def(py::init<mim::World*>())
        .def("write", &mim::PyWorld::write)
        .def("annex", &mim::PyWorld::annex, py::return_value_policy::reference_internal)
        .def("implicit_app", &mim::PyWorld::implicit_app, py::return_value_policy::reference_internal)
        .def("type_i32", &mim::PyWorld::type_i32, py::return_value_policy::reference_internal)
        .def("mut_fun", &mim::PyWorld::mut_fun, py::return_value_policy::reference_internal);

    // clang-format on
}

void init_world(py::module_& m) {
    py::class_<mim::World>(m, "World");
    // .def("write", static_cast<mim::Def* (mim::World::*)()>(&mim::World::write), py::return_value_policy::reference)
    // .def("annex", &mim::World::annex, py::return_value_policy::reference_internal)
    // .def("implicit_app", &mim::World::implicit_app, py::return_value_policy::reference)
    // .def("type_i32", &mim::World::type_i32, py::return_value_policy::reference_internal);
    // -        .def("write", static_cast<void (World::*)()>(&mim::World::write));
}
} // namespace mim
