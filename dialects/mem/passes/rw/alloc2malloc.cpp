#include "dialects/mem/passes/rw/alloc2malloc.h"

namespace thorin::mem {

const Def* Alloc2Malloc::rewrite(const Def* def) {
    // todo: move alloc and slot to dialect
    if (auto alloc = isa<Tag::Alloc>(def)) {
        auto [pointee, addr_space] = alloc->decurry()->args<2>();
        return world().op_malloc(pointee, alloc->arg(), alloc->dbg());
    } else if (auto slot = isa<Tag::Slot>(def)) {
        auto [pointee, addr_space] = slot->decurry()->args<2>();
        auto [mem, id]             = slot->args<2>();
        return world().op_mslot(pointee, mem, id, slot->dbg());
    }

    return def;
}

PassTag Alloc2Malloc::ID{};

} // namespace thorin::mem
