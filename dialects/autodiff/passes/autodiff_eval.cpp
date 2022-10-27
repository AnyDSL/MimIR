#include <iostream>
#include <thorin/dump.cpp>

#include <thorin/analyses/deptree.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
// #include "dialects/direct/direct.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/mem/mem.h"

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
        // callee = autodiff T
        // arg = function of type T
        //   (or operator)
        // auto callee = ad_app->callee();
        auto arg = ad_app->arg();
        world().DLOG("found a autodiff::autodiff of {}", arg);

        assert(arg->isa<Lam>());

        root = std::make_shared<LoopFrame>(*this);
        auto& w = world();
        LoopData data;
        root->size = one(w);
        root->local_size = one(w);
        data.index = zero(w);
        data.cache_index = zero(w);
        root->forward = data;
        root->backward = data;
        current_loop = root;

        def = derive(arg);
        return def;
    }

    return def;
}

} // namespace thorin::autodiff
