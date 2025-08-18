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
        world_->DLOG("given sym: {}", sym);

        auto ret = world_->sym2annex(sym);

        world_->DLOG("returned Def: {}", ret);
        return world_->sym2annex(sym);
    }

    const mim::Def* implicit_app(const mim::Def* calle, std::vector<mim::Def*> args) { return world_->implicit_app(calle, args); }

    const mim::Def* type_i32() { return world_->type_i32(); }

    void write() {
        world_->write();
        return;
    }

    Lam* mut_fun(const mim::Def* dom, std::vector<mim::Def*> codom) { return world_->mut_fun(dom, codom); }

private:
    mim::World* world_;
};

} // namespace mim
