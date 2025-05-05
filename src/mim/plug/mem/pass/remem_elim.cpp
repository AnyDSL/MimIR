#include "mim/plug/mem/pass/remem_elim.h"

#include "mim/plug/mem/mem.h"

namespace mim::plug::mem {

const Def* RememElim::rewrite(const Def* def) {
    if (auto remem = test<mem::remem>(def)) return remem->arg();
    return def;
}

} // namespace mim::plug::mem
