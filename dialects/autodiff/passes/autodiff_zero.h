#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::autodiff {

class AutoDiffZero : public RWPass<AutoDiffZero, Lam> {
public:
    AutoDiffZero(PassMan& man)
        : RWPass(man, "autodiff_zero") {}

    const Def* rewrite(const Def*) override;

    static PassTag* ID();

    const Def* mod_pb(const Def* def);

private:
};

} // namespace thorin::autodiff
