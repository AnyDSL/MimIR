#include "mim/phase/beta_red_phase.h"

namespace mim {

bool BetaRedPhase::analyze() {
    for (auto def : old_world().externals())
        analyze(def);
    return false; // no fixed-point neccessary
}

void BetaRedPhase::analyze(const Def* def) {
    if (auto [_, ins] = analyzed_.emplace(def); !ins) return;

    if (auto var = def->isa<Var>()) return visit(var->type()); // ignore Var's mut

    for (auto d : def->deps())
        visit(d);
}

void BetaRedPhase::visit(const Def* def) {
    if (auto lam = def->isa_mut<Lam>(); lam && !lam->is_external()) {
        if (auto [i, ins] = candidates_.emplace(lam, true); !ins) i->second = false;
    }
    analyze(def);
}

const Def* BetaRedPhase::rewrite_imm_App(const App* app) {
    if (auto old_lam = app->callee()->isa_mut<Lam>(); old_lam && old_lam->is_set() && is_candidate(old_lam)) {
        DLOG("beta-reduce: `{}`", old_lam);
        if (auto var = old_lam->has_var()) {
            auto new_arg = rewrite(app->arg());
            push();
            map(var, new_arg);
            auto res = rewrite(old_lam->body());
            pop();
            todo_ = true;
            return res;
        } else {
            todo_ = true;
            return rewrite(old_lam->body());
        }
    }

    return Rewriter::rewrite_imm_App(app);
}

bool BetaRedPhase::is_candidate(Lam* lam) const {
    if (lam->is_external()) return false;

    auto i = candidates_.find(lam);
    assert(i != candidates_.end());
    return i->second;
}

} // namespace mim
