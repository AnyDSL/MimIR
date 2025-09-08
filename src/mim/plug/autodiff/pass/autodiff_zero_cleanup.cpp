#include "mim/plug/autodiff/pass/autodiff_zero_cleanup.h"

#include <iostream>

#include <mim/lam.h>

#include "mim/plug/affine/affine.h"
#include "mim/plug/autodiff/autodiff.h"
#include "mim/plug/core/core.h"
#include "mim/plug/mem/mem.h"

namespace mim::plug::autodiff {

const Def* AutoDiffZeroCleanup::rewrite(const Def* def) {
    // Ideally this code segment is never reached.
    // (all zeros are resolved before)
    if (auto zero_app = Axm::isa<zero>(def); zero_app) {
        // callee = zero
        // arg = type T
        auto T = zero_app->arg();
        DLOG("found a remaining autodiff::zero of {}", T);
        // generate ⊥:T
        auto dummy = world().bot(T);
        return dummy;
    }

    return def;
}

} // namespace mim::plug::autodiff
