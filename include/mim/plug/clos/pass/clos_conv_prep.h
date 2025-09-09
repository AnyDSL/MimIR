#pragma once

#include <mim/pass.h>

#include "mim/plug/clos/clos.h"

namespace mim {

class EtaExp;

namespace plug::clos {

class ClosConvPrep : public RWPass<ClosConvPrep, Lam> {
public:
    ClosConvPrep(World& world, flags_t annex)
        : RWPass(world, annex) {}
    ClosConvPrep* recreate() final { return driver().stage<ClosConvPrep>(world(), annex()); }

    void init(PassMan*) final;

    void enter() override;
    const Def* rewrite(const Def*) override;
    const App* rewrite_arg(const App* app);
    const App* rewrite_callee(const App* app);

    Lam* scope(Lam* lam);

    bool from_outer_scope(Lam* lam) {
        // return curr_mut()->free_vars().intersects(lam->free_vars()); }
        // return scope_.free_defs().contains(lam);
        return scope(lam) && scope(lam) != scope(curr_mut());
    }

    bool from_outer_scope(const Def* lam) { return curr_mut()->free_vars().has_intersection(lam->free_vars()); }

    const Def* eta_wrap(const Def* def, attr a) {
        auto& w                = world();
        auto [entry, inserted] = old2wrapper_.emplace(def, nullptr);
        auto& wrapper          = entry->second;
        if (inserted) {
            wrapper = w.mut_lam(def->type()->as<Pi>());
            wrapper->app(false, def, wrapper->var());
            lam2fscope_[wrapper] = scope(curr_mut());
            wrapper_.emplace(wrapper);
        }
        return world().call(a, wrapper);
    }

private:
    EtaExp* eta_exp_;
    DefMap<Lam*> old2wrapper_;
    DefSet wrapper_;
    Lam2Lam lam2fscope_;
    bool ignore_ = false;
};

} // namespace plug::clos
} // namespace mim
