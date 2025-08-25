#include "mim/phase/eta_red_phase.h"

namespace mim {

const Def* EtaRedPhase::rewrite_mut_Lam(Lam* lam) {
    if (auto body = lam->eta_reduce()) {
        todo_ = true;
        return body;
    }
    return Rewriter::rewrite_imm_Lam(lam);
}

const Def* EtaRedPhase::rewrite_imm_Lam(const Lam* lam) {
    // TOOD
    return RWPhase::rewrite_imm_Lam(lam);
}

} // namespace mim
