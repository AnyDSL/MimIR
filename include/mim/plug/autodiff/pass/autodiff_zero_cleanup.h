#pragma once

#include <mim/def.h>
#include <mim/pass.h>

namespace mim::plug::autodiff {

/// Replaces remaining zeros (not resolvable) with ⊥.
class AutoDiffZeroCleanup : public RWPass<AutoDiffZeroCleanup, Lam> {
public:
    AutoDiffZeroCleanup(PassMan& man)
        : RWPass(man, "autodiff_zero_cleanup") {}

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::autodiff
