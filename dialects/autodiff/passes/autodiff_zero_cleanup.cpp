#include "dialects/autodiff/passes/autodiff_zero_cleanup.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/core/core.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* AutoDiffZeroCleanup::rewrite(const Def* def) {
    // Ideally this code segment is never reached.
    // (all zeros are resolved before)
    if (auto zero_app = match<zero>(def); zero_app) {
        // callee = zero
        // arg = type T
        auto T = zero_app->arg();
        world().DLOG("found a remaining autodiff::zero of {}", T);
        // generate ⊥:T
        auto dummy = world().bot(T);
        return dummy;
    }

    return def;
}

} // namespace thorin::autodiff
