#include "mim/normalize.h"

#include <fe/assert.h>

#include "mim/world.h"

namespace mim {

[[nodiscard]] int commute_(const Def* a, const Def* b) {
    auto& world = a->world();

    if (a == b) return 0;
    if (a->isa_imm() && b->isa_mut()) return -1;
    if (a->isa_mut() && b->isa_imm()) return +1;

    // clang-format off
    if (a->node()    != b->node()   ) return a->node()    < b->node()    ? -1 : +1;
    if (a->num_ops() != b->num_ops()) return a->num_ops() < b->num_ops() ? -1 : +1;
    if (a->flags()   != b->flags()  ) return a->flags()   < b->flags()   ? -1 : +1;
    // clang-format on

    if (a->isa_mut() && b->isa_mut()) {
        world.WLOG("resorting to unstable gid-based compare for commute check");
        return a->gid() < b->gid() ? -1 : +1;
    }

    assert(a->isa_imm() && b->isa_imm());

    if (auto var1 = a->isa<Var>()) {
        auto var2 = b->as<Var>();
        world.WLOG("resorting to unstable gid-based compare for commute check");
        return var1->gid() < var2->gid() ? -1 : +1;
    }

    // heuristic: iterate backwards as index (often a Lit) comes last and will faster find a solution
    for (size_t i = a->num_ops(); i-- != 0;)
        if (int cmp = commute_(a->op(i), b->op(i)); cmp != 0) return cmp;

    if (int cmp = commute_(a->type(), b->type()); cmp != 0) return cmp;

    fe::unreachable();
}

} // namespace mim
