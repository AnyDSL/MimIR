#include <iostream>

#include <thorin/lam.h>
#include <thorin/tables.h>

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
// #include "dialects/direct/direct.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/passes/autodiff_zero.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* AutoDiffZero::rewrite(const Def* def) {
    auto& world = def->world();
        world.DLOG("rewrite in zero pass {} : {}", def, def->type());
        // assert(0);
    if(def->isa<Axiom>()) {
        world.DLOG("found a axiom {}", def);
        // assert(0);
    }

    // TODO: why does the pass not descent into tuple, axiom applications (wrap_mul)
    if (auto zero_app = match<zero>(def); zero_app) {
        // callee = zero
        // arg = type T
        auto T = zero_app->arg();
        world.DLOG("found a autodiff::zero of {}", T);
        // assert(0);
        auto zero = zero_def(T);
        if(zero)
            return zero;
        return def;
    }

    return def;
}

PassTag* AutoDiffZero::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::autodiff
