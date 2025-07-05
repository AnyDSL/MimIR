#include <mim/world.h>
#include <fe/sym.h>
#include <pybind11/pybind11.h>
#include <mim/def.h>

namespace py = pybind11;
namespace mim{
    class PyWorld {
        private:
            mim::World* wrld_;
        public:
            PyWorld(mim::World* world){
                wrld_ = world;
            }

            const mim::Def* annex(Sym sym){
                wrld_->DLOG("given sym: {}", sym);

                auto ret =  wrld_->sym2annex(sym);

                wrld_->DLOG("returned Def: {}", ret);
                return wrld_->sym2annex(sym);
            }

            const mim::Def* implicit_app(const mim::Def* calle, mim::Defs args){
                return wrld_->implicit_app(calle, args);
            }

            const mim::Def* type_i32(){
                return wrld_->type_i32();
            }

            void write(){
                wrld_->write();
                return;
            }
        
            Lam* mut_fun(const mim::Def* dom, mim::Defs codom){
                return wrld_->mut_fun(dom, codom);
            }
    };
}
