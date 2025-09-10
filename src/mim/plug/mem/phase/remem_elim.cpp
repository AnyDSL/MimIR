#include "mim/plug/mem/phase/remem_elim.h"

#include "mim/plug/mem/mem.h"

namespace mim::plug::mem::phase {

const Def* RememElim::rewrite(const Def* def) {
    if (auto remem = Axm::isa<mem::remem>(def)) return rewrite(remem->arg());
    return Rewriter::rewrite(def);
}

} // namespace mim::plug::mem::phase
