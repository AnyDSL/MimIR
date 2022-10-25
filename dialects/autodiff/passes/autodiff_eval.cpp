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
    auto augmented_def = augment_(def, f, f_diff);
    augmented[def]     = augmented_def;
    return augmented[def];
}

const Def* AutoDiffEval::derive(const Def* def) {
    if (auto i = derived.find(def); i != derived.end()) return i->second;
    auto derived_def = derive_(def);
    derived[def]     = derived_def;
    return derived[def];
}

const Def* AutoDiffEval::rewrite(const Def* def) {
    if (auto ad_app = match<ad>(def); ad_app) {
        // callee = autodiff T
        // arg = function of type T
        //   (or operator)
        auto arg = ad_app->arg();
        world().DLOG("found a autodiff::autodiff of {}", arg);

        assert(arg->isa<Lam>() && "Only functions can currently be differentiated via axiom");
        // TODO: handle operators analogous
        def = derive(arg);
        return def;
    }

    return def;
}

} // namespace thorin::autodiff
