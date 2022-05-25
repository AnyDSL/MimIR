#include "dialects/mem/passes/rw/remem_elim.h"

namespace thorin::mem {

const Def* RememElim::rewrite(const Def* def) {
    // todo: move remem to dialect
    if (auto remem = isa<Tag::Remem>(def)) return remem->arg();
    return def;
}

PassTag RememElim::ID{};

} // namespace thorin::mem
