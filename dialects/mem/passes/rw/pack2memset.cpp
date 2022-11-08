#include "dialects/mem/passes/rw/pack2memset.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

std::pair<const App*, const Pack*> is_pack_store(const Def* def) {
    if (auto store = match<mem::store>(def)) {
        auto [_, ptr, val] = store->args<3>();

        if (auto pack = val->isa<Pack>()) {
            auto shape_def = pack->shape();
            if (auto lit = isa_lit(pack->body()); lit && *lit == 0 && !isa_lit(shape_def)) { return {store, pack}; }
        }
    }

    return {nullptr, nullptr};
}

const Def* Pack2Memset::rewrite(const Def* def) {
    if (auto [store, pack] = is_pack_store(def); store && pack) {
        auto [mem, ptr, val] = store->args<3>();
        auto& w              = world();
        auto body_size       = core::op(core::trait::size, pack->body()->type());
        auto size            = core::op(core::nop::mul, pack->shape(), body_size);
        auto ptr_cast        = core::op_bitcast(mem::type_ptr(w.type_int(8)), ptr);
        return op_memset(mem, ptr_cast, size, w.lit_int(8, 0));
    }

    return def;
}

} // namespace thorin::mem
