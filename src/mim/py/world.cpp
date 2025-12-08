#include <fe/sym.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <mim/def.h>
#include <mim/world.h>

#include <mim/pass/optimize.h>

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
        .def("mut_fun", &mim::PyWorld::mut_fun, py::return_value_policy::reference_internal)
        .def("sym", &mim::PyWorld::sym, py::return_value_policy::reference_internal);

    // clang-format on
}

void init_world(py::module_& m) {
    py::class_<mim::World, std::unique_ptr<mim::World, py::nodelete>>(m, "World")
        .def("write", py::overload_cast<>(&mim::World::write))
        .def("annex", &mim::World::sym2annex, py::return_value_policy::reference_internal)
        .def("implicit_app",
             static_cast<const mim::Def* (World::*)(const mim::Def*, mim::Defs)>(&mim::World::implicit_app),
             py::return_value_policy::reference_internal)
        .def("type_i32", &mim::World::type_i32, py::return_value_policy::reference_internal)
        .def("sym", static_cast<fe::Sym (World::*)(std::string_view)>(&mim::World::sym),
             py::return_value_policy::reference_internal)
        .def(
            "mut_fun2",
            [](mim::World& w, std::vector<mim::Def*> dom, std::vector<mim::Def*> codom) {
                auto d = dom;
                return w.mut_fun(mim::Defs(dom), mim::Defs(codom));
            },
            py::return_value_policy::reference_internal)
        .def(
            "mut_fun",
            [](mim::World& w, const mim::Def* dom, std::vector<mim::Def*> codom) {
                auto d = dom;

                std::cout << "called mut_fun with domain: " << d << std::endl;
                return w.mut_fun(dom, codom);
            },
            py::return_value_policy::reference_internal)
        .def("call",
             [](mim::World& w, fe::Sym sym, std::vector<mim::Def*> args) { return w.call_sym(sym, mim::Defs(args)); })
        .def("optimize", [](mim::World& w) { mim::optimize(w); });
    // .def("call", static_cast<const mim::Def* (World::*)(mim::Sym, py::args)>);
}
} // namespace mim
