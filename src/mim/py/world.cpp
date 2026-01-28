#include <fe/sym.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <mim/def.h>
#include <mim/world.h>
#include <mim/lam.h>
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
        .def("top_nat", &mim::World::top_nat, py::return_value_policy::reference_internal)
        .def("type_bool", &mim::World::type_bool, py::return_value_policy::reference_internal)
        .def("type_i8", &mim::World::type_i8, py::return_value_policy::reference_internal)
        .def("lit_nat_0", &mim::World::lit_nat_0, py::return_value_policy::reference_internal)
        .def("lit", &mim::World::lit, py::return_value_policy::reference_internal)
        .def("type_idx", 
            static_cast<const mim::Def* (World::*)(const mim::Def*)>(&mim::World::type_idx), py::return_value_policy::reference_internal
        )
        .def("type_idx", 
            static_cast<const mim::Def* (World::*)(nat_t)>(&mim::World::type_idx), py::return_value_policy::reference_internal
        )
        .def("cn",
            [](mim::World& w,std::vector<Def*> dom){
                return w.cn(Defs(dom));
            }
        )
        .def("lit_i8", 
            static_cast<const mim::Lit* (World::*)(u8)>(&mim::World::lit_i8), 
            py::return_value_policy::reference_internal
            )
        .def("implicit_app",
            [](mim::World& w, const mim::Def* callee, std::vector<Def*> args){
                return w.implicit_app(callee, Defs(args));
            },
            py::return_value_policy::reference_internal
            )
        .def("type_i32", &mim::World::type_i32, py::return_value_policy::reference_internal)
        .def("sym",
            static_cast<fe::Sym (World::*)(std::string_view)>(&mim::World::sym),
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

                //std::cout << "called mut_fun with domain: " << d << std::endl;
                return w.mut_fun(dom, codom);
            },
            py::return_value_policy::reference_internal)
        .def("call",
            [](mim::World& w, std::string sym, pybind11::object args) { 
                // return w.call_sym(w.sym(sym), args);
                // -----------------
                

                // -----------------
                // if (args.is_none()){
                //     return w.sym2annex(w.sym(sym));
                // }
                // auto defs = args.cast<std::vector<mim::Def*>>();
                // for (auto d : defs)
                // {
                //     std::cout << d << std::endl;
                // }
                
                // return w.call_sym(w.sym(sym), mim::Defs(defs));
            },
            pybind11::arg("sym"),
            pybind11::arg("args") = pybind11::none()
        )
        .def("arr",
            [](mim::World& w, Def* arity, Def* body){
                return w.arr(arity, body);
            },
            py::return_value_policy::reference_internal)
        .def("call_by_id",
            [](mim::World& w, uint64_t id, std::vector<Def*> args) {
                if (args.size()<1){
                    return w.annex(id);
                }
                // auto defs = args.cast<std::vector<mim::Def*>>();
                // for (auto d : defs)
                // {
                //     std::cout << d << std::endl;
                // }
                return w.call(id, mim::Defs(args));
            },
            pybind11::arg("sym"),
            pybind11::arg("args") = pybind11::none()
        )
        .def("optimize", [](mim::World& w) { 
            std::cout << "printing world externals: " << std::endl;
            for(auto[sym, whatever] :  w.externals().sym2mut()){
                std::cout << sym.str() << std::endl;
            }
            std::cout << "----" << std::endl;
            mim::optimize(w); })
        .def("dot", static_cast<void (World::*)(const char*, bool, bool) const>(&mim::World::dot))
        .def("mut_con", [](mim::World& w, std::vector<Def*> domains){
            return w.mut_con(Defs(domains));
        });
    // .def("call", static_cast<const mim::Def* (World::*)(mim::Sym, py::args)>);
}

/*
    // just do this the lambda I guess

    // testing some python stuff
    template<bool Normalize = true>
    const Def* call_(const Def* callee, py::args args){
        if (args.size()>1){
            auto first_arg = *args.begin();
            py::args remaining_vec = py::make_tuple(args.begin()+1);
            return call_<Normalize>(implicit_app(callee, first_arg, remaining_vec));
        }
        else{
            return implicit_app<Normalize>(callee, args);
        }
    }
*/
} // namespace mim