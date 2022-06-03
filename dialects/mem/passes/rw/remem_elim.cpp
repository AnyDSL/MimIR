#include "dialects/mem/passes/rw/remem_elim.h"

#include "dialects/mem.h"

namespace thorin::mem {

const Def* RememElim::rewrite(const Def* def) {
    if (auto remem = match<mem::remem>(def)) return remem->arg();
    return def;
}

PassTag* RememElim::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::mem
