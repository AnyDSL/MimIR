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
            void annex(Sym sym){
                wrld_->sym2annex(sym);
                return;
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
    };
}