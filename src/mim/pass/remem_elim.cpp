#include "mim/pass/rw/remem_elim.h"

namespace mim {

const Def* RememElim::rewrite(const Def* def) {
    if (auto remem = isa<Tag::Remem>(def)) return remem->arg();
    return def;
}

} // namespace mim
