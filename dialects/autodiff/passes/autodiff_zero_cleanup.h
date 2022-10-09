#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::autodiff {

/// Replaces remaining zeros (not resolvable) with ⊥.
class AutoDiffZeroCleanup : public RWPass<AutoDiffZeroCleanup, Lam> {
public:
    AutoDiffZeroCleanup(PassMan& man)
        : RWPass(man, "autodiff_zero_cleanup") {}

    const Def* rewrite(const Def*) override;
};

} // namespace thorin::autodiff
