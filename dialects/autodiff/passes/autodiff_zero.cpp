#include "dialects/autodiff/passes/autodiff_zero.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/core/core.h"
#include "dialects/mem/mem.h"

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
