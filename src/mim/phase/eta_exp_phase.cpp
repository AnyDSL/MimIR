#include "mim/phase/eta_exp_phase.h"

namespace mim {

const Def* EtaExpPhase::rewrite_imm(const Def* def) { return Rewriter::rewrite_imm(def); }

const Def* EtaExpPhase::rewrite_mut(Def* def) { return Rewriter::rewrite_mut(def); }

} // namespace mim
