#pragma once
#include <mim/def.h>
#include <mim/world.h>
#include "t_def.h"
#include "t_lam.h"
#include "t_sym.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace mim{
    class MIM_World{
        private:
            mim::World* _world;
        public:
            MIM_World(mim::World* wlrd)
            : _world(wlrd)
            {
                this->_world = wlrd;
            }

            // MIM_World(mim::World& wlrd)
            // {
            //     this->_world = wlrd;
            // }

            const mim::MIM_Def* annex(Sym sym) {
                MIM_Def* p = new MIM_Def(const_cast<mim::Def*>(this->_world->sym2annex(sym)));
                return p;
            }

            const mim::MIM_Def* implicit_app(const mim::Def* callee, std::vector<mim::Def*> args) {
                MIM_Def* p =  new MIM_Def(const_cast<mim::Def*>(this->_world->implicit_app(callee, args)));
                return p;
            }
            
            const mim::MIM_Def* type_i32() { return new MIM_Def(const_cast<mim::Def*>(this->_world->type_i32())); }
        
            void write() {
                this->_world->write();
                return;
            }
        
            // MIM_Lam* mut_fun(const mim::Def* dom, std::vector<mim::Def*> codom) {
            //     auto c = dom;
            //     std::cout  << "hello from cpp: " <<  c << std::endl;
            //     return MIM_Lam(this->_world->mut_fun(dom, codom)); 
            // }
        
            MIM_Sym sym(const std::string& s){
                return MIM_Sym(this->_world->sym(s));
            }

            MIM_Def call(MIM_Sym callee, py::args args){
                
                mim::Defs defs(args_inner);
                return MIM_Def(const_cast<mim::Def*>(this->_world->call(*callee.into_inner(), defs)));
            }


    };

    void init_MIM_World(py::module_& m){
        py::class_<mim::MIM_World>(m, "MIM_World")
            .def(py::init<mim::World*>())
            .def("call", &mim::MIM_World::call, py::return_value_policy::reference_internal)
            .def("sym", &mim::MIM_World::sym, py::return_value_policy::reference_internal);
    }
}