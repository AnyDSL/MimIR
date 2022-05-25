#include "thorin/pass/rw/alloc2malloc.h"

namespace thorin {

const Def* Alloc2Malloc::rewrite(const Def* def) {
    if (auto alloc = isa<Group::Alloc>(def)) {
        auto [pointee, addr_space] = alloc->decurry()->args<2>();
        return world().op_malloc(pointee, alloc->arg(), alloc->dbg());
    } else if (auto slot = isa<Group::Slot>(def)) {
        auto [pointee, addr_space] = slot->decurry()->args<2>();
        auto [mem, id]             = slot->args<2>();
        return world().op_mslot(pointee, mem, id, slot->dbg());
    }

    return def;
}

} // namespace thorin
