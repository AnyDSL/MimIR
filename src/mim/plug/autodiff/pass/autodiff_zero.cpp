#include "mim/plug/autodiff/pass/autodiff_zero.h"

#include <iostream>

#include <mim/lam.h>

#include "mim/plug/affine/affine.h"
#include "mim/plug/autodiff/autodiff.h"
#include "mim/plug/core/core.h"
#include "mim/plug/mem/mem.h"

namespace mim::plug::autodiff {

const Def* AutoDiffZero::rewrite(const Def* def) {
    if (auto zero_app = isa<zero>(def); zero_app) {
        // callee = zero
        // arg = type T
        auto T = zero_app->arg();
        world().DLOG("found a autodiff::zero of {}", T);
        auto zero = zero_def(T);
        if (zero) return zero;
        return def;
    }

    return def;
}

} // namespace mim::plug::autodiff
