#include "plug/mem/pass/rw/remem_elim.h"

#include "plug/mem/mem.h"

namespace thorin::mem {

Ref RememElim::rewrite(Ref def) {
    if (auto remem = match<mem::remem>(def)) return remem->arg();
    return def;
}

} // namespace thorin::mem
