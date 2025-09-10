#include "mim/plug/autodiff/pass/eval.h"

#include <mim/lam.h>

#include "mim/plug/autodiff/autodiff.h"

namespace mim::plug::autodiff {

// TODO: maybe use template (https://codereview.stackexchange.com/questions/141961/memoization-via-template) to memoize
const Def* Eval::augment(const Def* def, Lam* f, Lam* f_diff) {
    if (auto i = augmented.find(def); i != augmented.end()) return i->second;
    augmented[def] = augment_(def, f, f_diff);
    return augmented[def];
}

const Def* Eval::derive(const Def* def) {
    if (auto i = derived.find(def); i != derived.end()) return i->second;
    derived[def] = derive_(def);
    return derived[def];
}

const Def* Eval::rewrite(const Def* def) {
    if (auto ad_app = Axm::isa<ad>(def); ad_app) {
        // callee = autodiff T
        // arg = function of type T
        //   (or operator)
        auto arg = ad_app->arg();
        DLOG("found a autodiff::autodiff of {}", arg);

        if (arg->isa<Lam>()) return derive(arg);

        // TODO: handle operators analogous

        assert(0 && "not implemented");
        return def;
    }

    return def;
}

} // namespace mim::plug::autodiff
