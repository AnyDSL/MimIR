#include "mim/phase/eta_red_phase.h"

namespace mim {

void EtaRedPhase::rewrite_externals() {
    for (auto mut : old_world().copy_externals())
        mut->transfer_external(rewrite_no_eta(mut)->as_mut());
}

const Def* EtaRedPhase::rewrite(const Def* old_def) {
    if (auto lam = old_def->isa<Lam>()) {
        if (auto callee = lam->eta_reduce()) return todo_ = true, rewrite(callee);
    }

    return Rewriter::rewrite(old_def);
}

const Def* EtaRedPhase::rewrite_no_eta(const Def* old_def) { return Rewriter::rewrite(old_def); }

const Def* EtaRedPhase::rewrite_imm_Var(const Var* var) {
    return new_world().var(rewrite(var->type()), rewrite_no_eta(var->mut())->as_mut());
}

} // namespace mim
