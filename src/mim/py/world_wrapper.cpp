#include <mim/world.h>
#include <fe/sym.h>

namespace mim{
    class PyWorld {
        private:
            World wrld_;
        public:
            void annex(Sym sym){
                wrld_.sym2annex(sym);
                return;
            }
            Def* call(Sym sym, ){

            }
    };
}