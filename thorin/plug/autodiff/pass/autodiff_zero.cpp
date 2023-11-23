#include "thorin/plug/autodiff/pass/autodiff_zero.h"

#include <iostream>

#include <thorin/lam.h>

#include "thorin/plug/affine/affine.h"
#include "thorin/plug/autodiff/autodiff.h"
#include "thorin/plug/core/core.h"
#include "thorin/plug/mem/mem.h"

namespace thorin::plug::autodiff {

Ref AutoDiffZero::rewrite(Ref def) {
    if (auto zero_app = match<zero>(def); zero_app) {
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

} // namespace thorin::plug::autodiff
