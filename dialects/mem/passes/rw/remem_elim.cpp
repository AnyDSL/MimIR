#include "dialects/mem/passes/rw/remem_elim.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

const Def* RememElim::rewrite(const Def* def) {
    if (auto remem = match<mem::remem>(def)) return remem->arg();
    return def;
}

} // namespace thorin::mem
