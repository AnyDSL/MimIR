#include "mim/pass/eta_red.h"

namespace mim {

static const App* eta_rule(Lam* lam) {
    if (auto app = lam->body()->isa<App>()) {
        if (app->arg() == lam->var()) return app;
    }
    return nullptr;
}

const Def* EtaRed::rewrite(const Def* def) {
    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        // TODO (ClosureConv): Factor this out
        if (auto lam = def->op(i)->isa_mut<Lam>(); (!callee_only() || isa_callee(def, i)) && lam && lam->is_set()) {
            if (auto app = eta_rule(lam); app && !irreducible_.contains(lam)) {
                data().emplace(lam, Lattice::Reduce);
                auto new_def = def->refine(i, app->callee());
                DLOG("eta-reduction '{}' -> '{}' by eliminating '{}'", def, new_def, lam);
                return new_def;
            }
        }
    }

    return def;
}

undo_t EtaRed::analyze(const Var* var) {
    if (auto lam = var->mut()->isa_mut<Lam>()) {
        auto [_, l] = *data().emplace(lam, Lattice::Bot).first;
        auto succ   = irreducible_.emplace(lam).second;
        if (l == Lattice::Reduce && succ) {
            DLOG("irreducible: {}; found {}", lam, var);
            return undo_visit(lam);
        }
    }

    return No_Undo;
}

} // namespace mim
