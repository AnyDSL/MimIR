#include "thorin/plug/mem/pass/remem_elim.h"

#include "thorin/plug/mem/mem.h"

namespace thorin::plug::mem {

Ref RememElim::rewrite(Ref def) {
    if (auto remem = match<mem::remem>(def)) return remem->arg();
    return def;
}

} // namespace thorin::plug::mem
