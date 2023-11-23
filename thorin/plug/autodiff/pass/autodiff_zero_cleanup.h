#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::plug::autodiff {

/// Replaces remaining zeros (not resolvable) with ⊥.
class AutoDiffZeroCleanup : public RWPass<AutoDiffZeroCleanup, Lam> {
public:
    AutoDiffZeroCleanup(PassMan& man)
        : RWPass(man, "autodiff_zero_cleanup") {}

    Ref rewrite(Ref) override;
};

} // namespace thorin::plug::autodiff
