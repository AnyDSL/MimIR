#include "thorin/normalize.h"
#include "thorin/world.h"

#include "dialects/mem.h"

namespace thorin::normalize {

// clang-format off
template<nat_t w> struct Fold<mem::Div, mem::Div::sdiv, w> { static Res run(u64 a, u64 b) { using T = w2s<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) / r); } };
template<nat_t w> struct Fold<mem::Div, mem::Div::udiv, w> { static Res run(u64 a, u64 b) { using T = w2u<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) / r); } };
template<nat_t w> struct Fold<mem::Div, mem::Div::srem, w> { static Res run(u64 a, u64 b) { using T = w2s<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) % r); } };
template<nat_t w> struct Fold<mem::Div, mem::Div::urem, w> { static Res run(u64 a, u64 b) { using T = w2u<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) % r); } };
// clang-format on

} // namespace thorin::normalize

namespace thorin::mem {

const Def* normalize_lea(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world                = type->world();
    auto [ptr, index]          = arg->projs<2>();
    auto [pointee, addr_space] = match<Ptr, false>(ptr->type())->args<2>();

    if (auto a = isa_lit(pointee->arity()); a && *a == 1) return ptr;
    // TODO

    return world.raw_app(callee, {ptr, index}, dbg);
}

const Def* normalize_load(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world                = type->world();
    auto [mem, ptr]            = arg->projs<2>();
    auto [pointee, addr_space] = match<Ptr, false>(ptr->type())->args<2>();

    if (ptr->isa<Bot>()) return world.tuple({mem, world.bot(type->as<Sigma>()->op(1))}, dbg);

    // loading an empty tuple can only result in an empty tuple
    if (auto sigma = pointee->isa<Sigma>(); sigma && sigma->num_ops() == 0)
        return world.tuple({mem, world.tuple(sigma->type(), {}, dbg)});

    return world.raw_app(callee, {mem, ptr}, dbg);
}

const Def* normalize_remem(const Def* type, const Def* callee, const Def* mem, const Def* dbg) {
    auto& world = type->world();

    // if (auto m = match<remem>(mem)) mem = m;
    return world.raw_app(callee, mem, dbg);
}

const Def* normalize_store(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world          = type->world();
    auto [mem, ptr, val] = arg->projs<3>();

    if (ptr->isa<Bot>() || val->isa<Bot>()) return mem;
    if (auto pack = val->isa<Pack>(); pack && pack->body()->isa<Bot>()) return mem;
    if (auto tuple = val->isa<Tuple>()) {
        if (std::ranges::all_of(tuple->ops(), [](const Def* op) { return op->isa<Bot>(); })) return mem;
    }

    return world.raw_app(callee, {mem, ptr, val}, dbg);
}

template<Div op>
const Def* normalize_div(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world      = type->world();
    auto callee      = c->as<App>();
    auto [mem, a, b] = arg->projs<3>();
    auto w           = isa_lit(callee->arg());
    type             = type->as<Sigma>()->op(1); // peel of actual type
    auto make_res    = [&, mem = mem](const Def* res) { return world.tuple({mem, res}, dbg); };

    if (auto result = normalize::fold<Div, op>(world, type, callee, a, b, dbg)) return make_res(result);

    if (auto la = a->isa<Lit>()) {
        if (la == world.lit_int(*w, 0)) return make_res(la); // 0 / b -> 0 and 0 % b -> 0
    }

    if (auto lb = b->isa<Lit>()) {
        if (lb == world.lit_int(*w, 0)) return make_res(world.bot(type)); // a / 0 -> ⊥ and a % 0 -> ⊥

        if (lb == world.lit_int(*w, 1)) {
            switch (op) {
                case Div::sdiv: return make_res(a);                    // a / 1 -> a
                case Div::udiv: return make_res(a);                    // a / 1 -> a
                case Div::srem: return make_res(world.lit_int(*w, 0)); // a % 1 -> 0
                case Div::urem: return make_res(world.lit_int(*w, 0)); // a % 1 -> 0
                default: unreachable();
            }
        }
    }

    if (a == b) {
        switch (op) {
            case Div::sdiv: return make_res(world.lit_int(*w, 1)); // a / a -> 1
            case Div::udiv: return make_res(world.lit_int(*w, 1)); // a / a -> 1
            case Div::srem: return make_res(world.lit_int(*w, 0)); // a % a -> 0
            case Div::urem: return make_res(world.lit_int(*w, 0)); // a % a -> 0
            default: unreachable();
        }
    }

    return world.raw_app(callee, {mem, a, b}, dbg);
}

THORIN_mem_NORMALIZER_IMPL

} // namespace thorin::mem
