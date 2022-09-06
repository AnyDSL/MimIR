#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::direct {

class DS2CPS;

/// Should replace the callsites of rewritten ds functions f
/// with cps2ds f_cps
/// the substitutions are provided in ds2cps->get_subst()
/// Warning: the iteration order is:
/// ds2cps on main
/// ds_call on main
/// ds2cps on f
/// ds_call on f
/// => rewriting of f not known when visiting main
/// problems:
/// - order
/// - rewrite does not visit lambdas
class DSCall : public RWPass<DSCall, Lam> {
public:
    DSCall(PassMan& man, DS2CPS* ds2cps)
        : RWPass(man, "ds_call")
        , ds2cps_(ds2cps) {}

    void enter() override;

    // const Def* rewrite(const Def*) override;

private:
    // Def2Def rewritten_;
    // Def2Def rewritten_bodies_;
    // Lam* curr_lam_ = nullptr;
    DS2CPS* ds2cps_;
    std::vector<Lam*> lams;

    // void rewrite_lam(Lam* lam);
    // const Def* rewrite_(const Def*);
    // const Def* rewrite_inner(const Def*);
};

} // namespace thorin::direct
