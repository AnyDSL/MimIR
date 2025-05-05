#include "mim/plug/autodiff/pass/autodiff_eval.h"

#include <iostream>

#include <mim/lam.h>

#include "mim/plug/affine/affine.h"
#include "mim/plug/autodiff/autodiff.h"
#include "mim/plug/core/core.h"
#include "mim/plug/mem/mem.h"

namespace mim::plug::autodiff {

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
    if (auto ad_app = test<ad>(def); ad_app) {
        // callee = autodiff T
        // arg = function of type T
        //   (or operator)
        auto arg = ad_app->arg();
        world().DLOG("found a autodiff::autodiff of {}", arg);

        if (arg->isa<Lam>()) return derive(arg);

        // TODO: handle operators analogous

        assert(0 && "not implemented");
        return def;
    }

    return def;
}

} // namespace mim::plug::autodiff
