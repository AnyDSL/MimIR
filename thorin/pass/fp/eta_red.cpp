#include "thorin/pass/fp/eta_red.h"

namespace thorin {

namespace {
bool is_var(Ref def) {
    if (def->isa<Var>()) return true;
    if (auto extract = def->isa<Extract>()) return extract->tuple()->isa<Var>();
    return false;
}

// For now, only η-reduce `lam x.e x` if e is a @p Var or a @p Lam.
const App* eta_rule(Lam* lam) {
    if (auto app = lam->body()->isa<App>()) {
        if (app->arg() == lam->var() && (is_var(app->callee()) || app->callee()->isa<Lam>())) return app;
    }
    return nullptr;
}
}

Ref EtaRed::rewrite(Ref def) {
    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        // TODO (ClosureConv): Factor this out
        if (auto lam = def->op(i)->isa_mut<Lam>(); (!callee_only_ || isa_callee(def, i)) && lam && lam->is_set()) {
            if (auto app = eta_rule(lam); app && !irreducible_.contains(lam)) {
                data().emplace(lam, Lattice::Reduce);
                auto new_def = def->refine(i, app->callee());
                world().DLOG("eta-reduction '{}' -> '{}' by eliminating '{}'", def, new_def, lam);
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
            world().DLOG("irreducible: {}; found {}", lam, var);
            return undo_visit(lam);
        }
    }

    return No_Undo;
}

} // namespace thorin
