#include "mim/phase/branch_normalize.h"

namespace mim {

const Def* BranchNormalizePhase::rewrite_mut_Lam(Lam* lam) { return Rewriter::rewrite_mut_Lam(lam); }

} // namespace mim
