#include "mim/plug/mem/phase/remem_elim.h"

#include "mim/plug/mem/mem.h"

namespace mim::plug::mem::phase {

const Def* RememElim::replace(const Def* def) {
    if (auto remem = Axm::isa<mem::remem>(def)) return remem->arg();
    return {};
}

} // namespace mim::plug::mem::phase
