#pragma once

#include <mim/def.h>
#include <mim/pass.h>

namespace mim::plug::autodiff {

/// Replaces calls to the zero axms with actual zeros.
class AutoDiffZero : public RWPass<AutoDiffZero, Lam> {
public:
    AutoDiffZero(World& world, flags_t annex)
        : RWPass(world, annex) {}

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::autodiff
