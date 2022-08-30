#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::autodiff {

class AutoDiffZeroCleanup : public RWPass<AutoDiffZeroCleanup, Lam> {
public:
    AutoDiffZeroCleanup(PassMan& man)
        : RWPass(man, "autodiff_zero_cleanup") {}

    const Def* rewrite(const Def*) override;

    static PassTag* ID();

    const Def* mod_pb(const Def* def);

private:
};

} // namespace thorin::autodiff
