#include "mim/phase/beta_red_phase.h"

namespace mim {

bool BetaEtaRedPhase::analyze() {
    for (auto def : world().externals())
        analyze(def);
    return false; // no fixed-point neccessary
}

void BetaEtaRedPhase::analyze(const Def* def) {
    if (auto [_, ins] = analyzed_.emplace(def); !ins) return;

    for (auto d : def->deps()) {
        if (auto lam = d->isa_mut<Lam>()) inc(lam);
        analyze(def);
    }
}

const Def* BetaEtaRedPhase::rewrite_imm_App(const App* app) {
    if (auto old_lam = app->callee()->isa_mut<Lam>(); old_lam && is_candidate(old_lam)) {
        if (auto var = old_lam->has_var()) {
            auto new_arg = rewrite(app->arg());
            push();
            map(var, new_arg);
            auto res = rewrite(old_lam->body());
            pop();
            return res;
        } else {
            return rewrite(old_lam->body());
        }
        todo_ = true;
    }

    return rewrite_imm(app);
}

} // namespace mim
