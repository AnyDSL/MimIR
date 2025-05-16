#include "mim/world.h"

#include "mim/plug/ord/ord.h"

#include "fe/assert.h"

namespace mim::plug::ord {

template<init id> const Def* normalize_init(const Def* type, const Def* callee, const Def* arg) {
    auto& world = type->world();
    return world.raw_app(type, callee, arg);
}

template<size> const Def* normalize_size(const Def*, const Def*, const Def* arg) {
    if (auto init = Axm::isa<ord::init>(arg)) return init->decurry()->arg();
    return nullptr;
}

const Def* normalize_get(const Def*, const Def*, const Def* arg) {
    auto [c, k] = arg->projs<2>();

    if (auto init = Axm::isa<ord::init>(c)) {
        if (auto tuple = init->arg()->isa<Tuple>()) {
            for (auto kv : tuple->ops())
                if (kv->proj(2, 0) == k) return kv->proj(2, 1);
        }
    }

    return nullptr;
}

template<mem id> const Def* normalize_mem(const Def*, const Def*, const Def* arg) {
    auto& w     = arg->world();
    auto [k, c] = arg->projs<2>();

    if (auto init = Axm::isa<ord::init>(c)) {
        if (auto tuple = init->arg()->isa<Tuple>()) {
            for (auto kv : tuple->ops()) {
                auto key = id == mem::map ? kv->proj(2, 0) : kv;
                if (key == k) return w.lit_tt();
            }
            return tuple->is_closed() ? w.lit_ff() : nullptr;
        }

        if (auto pack = init->arg()->isa_imm<Pack>()) w.WLOG("packs not yet implemented: {}", pack);
    }

    return nullptr;
}

template<insert id> const Def* normalize_insert(const Def*, const Def*, const Def*) {
    return nullptr;
#if 0
    auto& world    = type->world();
    auto [c, kv] = arg->projs<2>();

    if (auto init = Axm::isa<ord::init>(c)) {
        if (auto tuple = init->arg()->isa<Tuple>()) {
            auto n  = init->decurry()->arg();
            auto KV = init->decurry()->decurry()->arg();
            if (auto l = Lit::isa(n)) {
                DefVec new_ops;
                bool updated = false;
                for (size_t i = 0, e = *l; i != e; ++i) {
                    auto key = id == mem::map ? kv->proj(2, 0) : kv;
                    auto kv = tuple->proj(e, i);
                    if (kv->proj(2, 0) == k) {
                        updated = true;
                        new_ops.emplace_back(world.tuple({k, v}));
                    } else {
                        new_ops.emplace_back(tuple->proj(e, i));
                    }
                }
                if (!updated) new_ops.emplace_back(world.tuple({k, v}));
                return world.call(id, KV, new_ops.size(), Defs(new_ops));
            }
        }

        if (auto pack = init->arg()->isa_imm<Pack>()) w.WLOG("packs not yet implemented: {}", pack);
    }

    return world.raw_app(type, callee, arg);
#endif
}

MIM_ord_NORMALIZER_IMPL

} // namespace mim::plug::ord
