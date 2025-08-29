#include "mim/phase/eta_red_phase.h"

namespace mim {

void EtaRedPhase::start() {
    for (auto def : world().annexes())
        rewrite(def);

    for (auto old_mut : world().copy_externals()) {
        auto new_mut = rewrite(old_mut);

        if (auto old_lam = old_mut->isa<Lam>()) {
            if (new_mut->sym() != old_lam->sym()) {
                // we have eta-reduced an external so eta-expand it again
                new_mut = Lam::eta_expand(rewrite(old_lam->filter()), new_mut);
                new_mut->set<true>(old_lam->dbg());
            }
        }
        old_mut->transfer_external(new_mut->as_mut());
    }
}

const Def* EtaRedPhase::rewrite_mut_Lam(Lam* lam) {
    if (auto callee = lam->eta_reduce()) return todo_ = true, map(lam, rewrite(callee));
    return Rewriter::rewrite_mut_Lam(lam);
}

// TODO maybe we can eta-reduce immutable Lams in some edge casess like: lm _: [] = f ();

} // namespace mim
