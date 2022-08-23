#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::autodiff {

class AutoDiffEval : public RWPass<AutoDiffEval,Lam> {
public:
    AutoDiffEval(PassMan& man)
        : RWPass(man, "autodiff_eval") {}

    /// custom rewrite function
    /// memoized version of rewrite_
    const Def* rewrite(const Def*) override;
    const Def* rewrite_(const Def*);

    static PassTag* ID();

    const Def* mod_pb(const Def* def);

private:
    Def2Def rewritten;
    Def2Def app_pb;
};

} // namespace thorin::autodiff
