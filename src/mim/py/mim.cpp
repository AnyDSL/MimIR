#include "t_world.h"

#include <mim/ast/parser.h>
#include <mim/ast/ast.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <mim/driver.h>
#include <mim/world.h>

#include "t_world.h"

namespace py = pybind11;

namespace mim{
    class MIM{
        private:
            mim::Driver* driver;

        public:

            MIM(){
                this->driver = new Driver();

            }

            // make this return a MIM_World later
            MIM_World init(py::args args){
                fs::path path("../../../../build/lib/mim/");
                this->driver->add_search_path(path);
                this->driver->log().set(&std::cerr).set(Log::Level::Debug);
                auto ast = ast::AST(this->driver->world());
                auto parser = ast::Parser(ast);
                for(auto arg : args){
                    auto s = py::cast<std::string>(arg);
                    auto mod = parser.import(s);
                    mod->compile(ast);
                }
                return MIM_World(&this->driver->world());
            }
    };

    void init_MIM(py::module_& m) {
        py::class_<mim::MIM>(m, "Mim")
            .def(py::init<>())
            .def("init", &mim::MIM::init, py::return_value_policy::reference_internal);
    }

}


