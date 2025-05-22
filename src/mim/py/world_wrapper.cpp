#include <mim/world.h>
#include <fe/sym.h>
#include <pybind11/pybind11.h>
namespace py = pybind11;
namespace mim{
    class PyWorld {
        private:
            World* wrld_;
        public:
            PyWorld(World* world){
                wrld_ = world;
            }

            const Def* annex(Sym sym){
                return wrld_->sym2annex(sym);
            }

            const Def* implicit_app(const Def* calle, Defs args){
                return wrld_->implicit_app(calle, args);
            }

            const Def* type_i32(){
                return wrld_->type_i32();
            }

            void write(){
                wrld_->write();
                return;
            }
        
            Lam* mut_fun(const Def* dom, Defs codom){
                return wrld_->mut_fun(dom, codom);
            }
    };
}