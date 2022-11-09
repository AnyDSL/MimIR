#include "dialects/mem/passes/rw/pack2memset.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

bool is_zero(const Def* def) {
    if (auto lit = isa_lit(def)) {
        return *lit == 0;
    } else if (auto bitcast = match<core::bitcast>(def)) {
        return is_zero(bitcast->arg());
    }

    return false;
}

std::pair<const App*, const Pack*> is_pack_store(const Def* def) {
    if (auto store = match<mem::store>(def)) {
        auto [_, ptr, val] = store->args<3>();

        if (auto pack = val->isa<Pack>()) {
            auto shape_def = pack->shape();
            if (is_zero(pack->body())) { return {store, pack}; }
        }
    }

    return {nullptr, nullptr};
}

const Def* Pack2Memset::rewrite(const Def* def) {
    if (auto [store, pack] = is_pack_store(def); store && pack) {
        auto [mem, ptr, val] = store->args<3>();
        auto& w              = world();
        auto size            = core::op(core::trait::size, pack->type());
        auto ptr_cast        = core::op_bitcast(mem::type_ptr(w.type_int(8)), ptr);
        return op_memset(mem, ptr_cast, size, w.lit_int(8, 0));
    }

    return def;
}

} // namespace thorin::mem
