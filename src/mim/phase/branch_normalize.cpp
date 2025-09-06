#include "mim/phase/branch_normalize.h"

#include "mim/lam.h"

namespace mim {

const Def* BranchNormalizePhase::rewrite_mut_Lam(Lam* lam) {
    if (lam->is_set()) {
        if (auto br = Branch(lam->body())) {
            auto tt = br.tt()->isa<Lam>() ? br.tt() : Lam::eta_expand(br.tt());
            auto ff = br.ff()->isa<Lam>() ? br.ff() : Lam::eta_expand(br.ff());

            if (tt != br.tt() || ff != br.ff()) {
                DLOG("branch-noramlize - tt: `{} -> `{}` -- ff: `{}` -> `{}`", br.tt(), tt, br.ff(), ff);
                lam->branch(lam->filter(), br.cond(), tt, ff, br.arg());
            }
        }
    }

    return Rewriter::rewrite_mut_Lam(lam);
}

} // namespace mim
