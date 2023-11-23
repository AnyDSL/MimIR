#include "plug/autodiff/pass/autodiff_zero.h"

#include <iostream>

#include <thorin/lam.h>

#include "plug/affine/affine.h"
#include "plug/autodiff/autodiff.h"
#include "plug/core/core.h"
#include "plug/mem/mem.h"

namespace thorin::autodiff {

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

} // namespace thorin::autodiff
