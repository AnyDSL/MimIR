#pragma once

#include "thorin/pass/pass.h"

#include "dialects/clos/clos.h"

namespace thorin {

class EtaExp;

namespace clos {

class ClosConvPrep : public RWPass<ClosConvPrep, Lam> {
public:
    ClosConvPrep(PassMan& man, EtaExp* eta_exp)
        : RWPass(man, "eta_cont")
        , eta_exp_(eta_exp)
        , old2wrapper_()
        , lam2fscope_()
        , sym_{
            .free_ret     = world().sym("free_ret"),
            .eta_cont     = world().sym("eta_cont"),
            .fstclass_ret = world().sym("fstclass_ret"),
            .eta_br       = world().sym("eta_br") } {}

    void enter() override;
    const Def* rewrite(const Def*) override;
    const App* rewrite_arg(const App* app);
    const App* rewrite_callee(const App* app);

    Lam* scope(Lam* lam);

    bool from_outer_scope(Lam* lam) {
        // return scope_.free_defs().contains(lam);
        return scope(lam) && scope(lam) != scope(curr_nom());
    }

    bool from_outer_scope(const Def* lam) { return scope_->free_defs().contains(lam); }

    const Def* eta_wrap(const Def* def, attr a) {
        auto& w                = world();
        auto [entry, inserted] = old2wrapper_.emplace(def, nullptr);
        auto& wrapper          = entry->second;
        if (inserted) {
            wrapper = w.nom_lam(def->type()->as<Pi>());
            wrapper->app(false, def, wrapper->var());
            lam2fscope_[wrapper] = scope(curr_nom());
            wrapper_.emplace(wrapper);
        }
        return op(a, wrapper);
    }

private:
    EtaExp* eta_exp_;
    DefMap<Lam*> old2wrapper_;
    DefSet wrapper_;
    Lam2Lam lam2fscope_;
    std::unique_ptr<Scope> scope_;
    bool ignore_ = false;
    struct {
        Sym free_ret;
        Sym eta_cont;
        Sym fstclass_ret;
        Sym eta_br;
    } sym_;
};

} // namespace clos

} // namespace thorin
