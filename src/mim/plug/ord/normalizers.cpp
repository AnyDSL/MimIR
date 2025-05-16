#include "mim/world.h"

#include "mim/plug/ord/ord.h"

namespace mim::plug::ord {

template<init id> const Def* normalize_init(const Def* type, const Def* callee, const Def* arg) {
    auto& world = type->world();
    return world.raw_app(type, callee, arg);
}

template<size> const Def* normalize_size(const Def*, const Def*, const Def* arg) {
    if (auto init = Axm::isa<ord::init>(arg)) return init->decurry()->arg();
    return nullptr;
}

const Def* normalize_get(const Def* type, const Def* callee, const Def* arg) {
    auto& world = type->world();
    auto [c, k] = arg->projs<2>();

    if (auto init = Axm::isa<ord::init>(c)) {
        if (auto tuple = init->arg()->isa<Tuple>()) {
            for (auto kv : tuple->ops())
                if (kv->proj(2, 0) == k) return kv->proj(2, 1);
        }
    }
    return world.raw_app(type, callee, arg);
}

template<mem> const Def* normalize_mem(const Def* type, const Def* callee, const Def* arg) {
    auto& world = type->world();
    auto [c, k] = arg->projs<2>();
    if (auto init = Axm::isa<ord::init>(c)) {
        if (auto tuple = init->arg()->isa<Tuple>()) {
            bool all_keys_lit = true;
            for (auto kv : tuple->ops()) {
                auto key = kv->proj(2, 0);
                if (key == k) return world.lit_tt();
                if (!key->isa<Lit>()) key->dump();
                all_keys_lit &= (bool)key->isa<Lit>();
            }
            if (all_keys_lit) return world.lit_ff();
        }
    }
    return world.raw_app(type, callee, arg);
}

template<insert id> const Def* normalize_insert(const Def* type, const Def* callee, const Def* arg) {
    auto& world    = type->world();
    auto [c, k, v] = arg->projs<3>();

    if (auto init = Axm::isa<ord::init>(c)) {
        if (auto tuple = init->arg()->isa<Tuple>()) {
            auto n  = init->decurry()->arg();
            auto KV = init->decurry()->decurry()->arg();
            if (auto l = Lit::isa(n)) {
                DefVec new_ops;
                bool updated = false;
                for (size_t i = 0, e = *l; i != e; ++i) {
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
    }

    return world.raw_app(type, callee, arg);
}

MIM_ord_NORMALIZER_IMPL

} // namespace mim::plug::ord
