#include "mim/plug/mem/pass/alloc2malloc.h"

#include "mim/plug/mem/mem.h"

namespace mim::plug::mem {

const Def* Alloc2Malloc::rewrite(const Def* def) {
    if (auto alloc = Axm::isa<mem::alloc>(def)) {
        auto [pointee, addr_space] = alloc->decurry()->args<2>();
        return op_malloc(pointee, alloc->arg());
    } else if (auto slot = Axm::isa<mem::slot>(def)) {
        auto [pointee, addr_space] = slot->decurry()->args<2>();
        auto [mem, id]             = slot->args<2>();
        return op_mslot(pointee, mem, id);
    }

    return def;
}

} // namespace mim::plug::mem
