#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::autodiff {

/// Replaces calls to the zero axioms with actual zeros.
class AutoDiffZero : public RWPass<AutoDiffZero, Lam> {
public:
    AutoDiffZero(PassMan& man)
        : RWPass(man, "autodiff_zero") {}

    const Def* rewrite(const Def*) override;
};

} // namespace thorin::autodiff
