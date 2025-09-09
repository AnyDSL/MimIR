#pragma once

#include <mim/def.h>
#include <mim/pass.h>

namespace mim::plug::direct {

/// This is the second part of ds2cps.
/// We replace all ds call sites of cps (or ds converted) functions with the cps calls.
/// `b = f args` becomes `f (args,cont)` with a newly introduced continuation `cont: Cn b`.
class CPS2DS : public RWPass<CPS2DS, Lam> {
public:
    CPS2DS(World& world, flags_t annex)
        : RWPass(world, annex) {}
    CPS2DS* recreate() final { return driver().stage<CPS2DS>(world(), annex()); }

    void enter() override;

private:
    Def2Def rewritten_;
    DefSet rewritten_lams;
    Vector<Lam*> lam_stack;
    Lam* curr_lam_ = nullptr;

    void rewrite_lam(Lam* lam);
    const Def* rewrite_body(const Def*);
    const Def* rewrite_body_(const Def*);
};

} // namespace mim::plug::direct
