#include "mim/phase/eta_red_phase.h"

namespace mim {

const Def* EtaRedPhase::rewrite_mut_Lam(Lam* lam) {
    if (!lam->is_external())
        if (auto callee = lam->eta_reduce()) return todo_ = true, map(lam, rewrite(callee));

    return Rewriter::rewrite_mut_Lam(lam);
}

// TODO maybe we can eta-reduce immutable Lams in some edge casess like: lm _: [] = f ();

} // namespace mim
