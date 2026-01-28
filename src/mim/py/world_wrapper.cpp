#include <fe/sym.h>
#include <pybind11/pybind11.h>

#include <mim/def.h>
#include <mim/world.h>

namespace py = pybind11;
namespace mim {

class PyWorld {
public:
    PyWorld(mim::World* world)
        : world_(world) {}

    const mim::Def* annex(Sym sym) {
        return world_->sym2annex(sym);
    }

    const mim::Def* implicit_app(const mim::Def* calle, std::vector<mim::Def*> args) {
        return world_->implicit_app(calle, args);
    }

    const mim::Def* type_i32() { return world_->type_i32(); }

    void write() {
        world_->write();
        return;
    }

    Lam* mut_fun(const mim::Def* dom, std::vector<mim::Def*> codom) {
        auto c = dom;
        std::cout  << "hello from cpp: " <<  c << std::endl;
        return world_->mut_fun(dom, codom); 
    }

    Sym sym(const std::string& s){
        return world_->sym(s);
    }

private:
    mim::World* world_;
};

} // namespace mim
