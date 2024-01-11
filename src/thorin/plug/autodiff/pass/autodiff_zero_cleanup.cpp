#include "thorin/plug/autodiff/pass/autodiff_zero_cleanup.h"

#include <iostream>

#include <thorin/lam.h>

#include "thorin/plug/affine/affine.h"
#include "thorin/plug/autodiff/autodiff.h"
#include "thorin/plug/core/core.h"
#include "thorin/plug/mem/mem.h"

namespace thorin::plug::autodiff {

Ref AutoDiffZeroCleanup::rewrite(Ref def) {
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

} // namespace thorin::plug::autodiff
