#include "dialects/autodiff/passes/autodiff_eval.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/core/core.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

// TODO: maybe use template (https://codereview.stackexchange.com/questions/141961/memoization-via-template) to memoize
const Def* AutoDiffEval::augment(const Def* def, Lam* f, Lam* f_diff) {
    if (auto i = augmented.find(def); i != augmented.end()) return i->second;
    augmented[def] = augment_(def, f, f_diff);
    return augmented[def];
}

const Def* AutoDiffEval::derive(const Def* def) {
    if (auto i = derived.find(def); i != derived.end()) return i->second;
    derived[def] = derive_(def);
    return derived[def];
}

const Def* AutoDiffEval::rewrite(const Def* def) {
    if (auto ad_app = match<ad>(def); ad_app) {
        // callee = autodiff T
        // arg = function of type T
        //   (or operator)
        auto arg = ad_app->arg();
        world().DLOG("found a autodiff::autodiff of {}", arg);

        if (arg->isa<Lam>()) { return derive(arg); }

        // TODO: handle operators analogous

        assert(0 && "not implemented");
        return def;
    }

    return def;
}

} // namespace thorin::autodiff
