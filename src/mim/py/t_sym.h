#pragma once
#include <fe/sym.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace mim{
    class MIM_Sym{
        private:
            fe::Sym* _sym;

        public:
            MIM_Sym(fe::Sym* s){
                this->_sym = s;
            }

            MIM_Sym(fe::Sym s){
                this->_sym = &s;
            }

            bool empty(){
                return this->_sym->empty();
            }

            size_t size(){
                return this->_sym->size();
            }

            std::string_view  view(){
                return this->_sym->view();
            }

            std::string str(){
                return this->_sym->str();
            }

            fe::Sym* into_inner(){
                return this->_sym;
            }
    };


    void init_MIM_Sym(py::module_& m){
        py::class_<mim::MIM_Sym>(m, "MIM_Sym")
            .def(py::init<mim::Sym*>());
    }
}