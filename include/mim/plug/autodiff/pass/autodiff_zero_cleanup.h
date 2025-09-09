#pragma once

#include <mim/def.h>
#include <mim/pass.h>

namespace mim::plug::autodiff {

/// Replaces remaining zeros (not resolvable) with ⊥.
class AutoDiffZeroCleanup : public RWPass<AutoDiffZeroCleanup, Lam> {
public:
    AutoDiffZeroCleanup(World& world, flags_t annex)
        : RWPass(world, annex) {}
    AutoDiffZeroCleanup* recreate() final { return driver().stage<AutoDiffZeroCleanup>(world(), annex()); }

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::autodiff
