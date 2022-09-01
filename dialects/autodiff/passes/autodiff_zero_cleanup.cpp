#include <iostream>

#include <thorin/lam.h>
#include <thorin/tables.h>

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
// #include "dialects/direct/direct.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/passes/autodiff_zero_cleanup.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* AutoDiffZeroCleanup::rewrite(const Def* def) {
    // ideally never reached
    if (auto zero_app = match<zero>(def); zero_app) {
        // callee = zero
        // arg = type T
        auto T = zero_app->arg();
        world().DLOG("found a remaining autodiff::zero of {}", T);
        // generate ⊥:T
        // auto dummy = world().bot(T);
        // // assert(0);
        // return dummy;
        return def;
    }

    return def;
}

} // namespace thorin::autodiff
