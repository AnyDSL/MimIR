#include "mim/plug/mem/phase/alloc2malloc.h"

#include "mim/plug/mem/mem.h"

namespace mim::plug::mem::phase {

const Def* Alloc2Malloc::rewrite(const Def* def) {
    if (auto alloc = Axm::isa<mem::alloc>(def)) {
        auto [pointee, addr_space] = alloc->decurry()->args<2>();
        return rewrite(op_malloc(pointee, alloc->arg()));
    } else if (auto slot = Axm::isa<mem::slot>(def)) {
        auto [Ta, mi]              = slot->uncurry_args<2>();
        auto [pointee, addr_space] = Ta->projs<2>();
        auto [mem, id]             = mi->projs<2>();
        return rewrite(op_mslot(pointee, mem, id));
    }

    return Rewriter::rewrite(def);
}

} // namespace mim::plug::mem::phase
