#include "dialects/autodiff/passes/autodiff_eval.h"

#include <iostream>
#include <thorin/dump.cpp>

#include <thorin/analyses/deptree.h>
#include <thorin/lam.h>

#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/utils/helper.h"

namespace thorin::autodiff {

LoopData& LoopFrame::data() {
    if (autodiff_.current_state == State::Augment) {
        return forward;
    } else if (autodiff_.current_state == State::Invert) {
        return backward;
    }

    thorin::unreachable();
}

const Def* AutoDiffEval::augment(const Def* def) {
    const Def* augment;
    if (augment = augmented[def]; !augment) {
        assert(current_state == State::Augment);
        augment        = augment_(def);
        augmented[def] = augment;

        if (requires_caching(def)) { preserve(def, augment); }

        // be sure to cache extracts
        for (auto proj : def->projs()) { this->augment(proj); }
    }
    assert(augment);
    return augment;
}

const Def* AutoDiffEval::invert(const Def* def) {
    const Def* invert;
    if (invert = inverted[def]; !invert) {
        assert(current_state == State::Invert);
        invert = invert_(def);
        add_inverted(def, invert);
    }
    assert(invert);
    return invert;
}

const Def* AutoDiffEval::derive(const Def* def) {
    if (auto i = derived.find(def); i != derived.end()) return i->second;
    derived[def] = derive_(def);
    return derived[def];
}

const Def* AutoDiffEval::rewrite(const Def* def) {
    if (auto ad_app = match<ad>(def)) {
        auto arg = ad_app->arg();
        world().DLOG("found a autodiff::autodiff of {}", arg);
        assert(arg->isa<Lam>());

        for (auto use : ad_app->uses()) {
            if (auto app = use->isa<App>()) { app->arg()->dump(1); }
        }

        def = derive(arg);
        return def;
    }

    return def;
}

} // namespace thorin::autodiff
