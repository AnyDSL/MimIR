#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/pass/pass.h>

#include "thorin/plug/clos/clos.h"

namespace thorin {

class EtaExp;

namespace plug::clos {

class ClosConvPrep : public RWPass<ClosConvPrep, Lam> {
public:
    ClosConvPrep(PassMan& man, EtaExp* eta_exp)
        : RWPass(man, "clos_conv_prep")
        , eta_exp_(eta_exp)
        , old2wrapper_()
        , lam2fscope_() {}

    void enter() override;
    Ref rewrite(Ref) override;
    const App* rewrite_arg(const App* app);
    const App* rewrite_callee(const App* app);

    Lam* scope(Lam* lam);

    bool from_outer_scope(Lam* lam) {
        // return scope_.free_defs().contains(lam);
        return scope(lam) && scope(lam) != scope(curr_mut());
    }

    bool from_outer_scope(Ref lam) { return scope_->free_defs().contains(lam); }

    Ref eta_wrap(Ref def, attr a) {
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
    std::unique_ptr<Scope> scope_;
    bool ignore_ = false;
};

} // namespace plug::clos
} // namespace thorin
