#pragma once

#include <mim/def.h>
#include <mim/pass/pass.h>

namespace mim::plug::autodiff {

/// Replaces calls to the zero axioms with actual zeros.
class AutoDiffZero : public RWPass<AutoDiffZero, Lam> {
public:
    AutoDiffZero(PassMan& man)
        : RWPass(man, "autodiff_zero") {}

    Ref rewrite(Ref) override;
};

} // namespace mim::plug::autodiff
