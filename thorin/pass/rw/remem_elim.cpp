#include "thorin/pass/rw/remem_elim.h"

namespace thorin {

const Def* RememElim::rewrite(const Def* def) {
    if (auto remem = isa<Group::Remem>(def)) return remem->arg();
    return def;
}

} // namespace thorin
