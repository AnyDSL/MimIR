#include <thorin/normalize.h>
#include <thorin/world.h>

#include "thorin/plug/mem/mem.h"

namespace thorin::plug::mem {

namespace {

// TODO move to normalize.h or so?
// Swap Lit to left - or smaller gid, if no lit present.
template<class Id> void commute(Id id, const Def*& a, const Def*& b) {
    if (::thorin::is_commutative(id)) {
        if (b->isa<Lit>() || (a->gid() > b->gid() && !a->isa<Lit>())) std::swap(a, b);
    }
}

// clang-format off
template<class Id, Id id, nat_t w>
Res fold(u64 a, u64 b) {
    using ST = w2s<w>;
    using UT = w2u<w>;
    auto s = thorin::bitcast<ST>(a), t = thorin::bitcast<ST>(b);
    auto u = thorin::bitcast<UT>(a), v = thorin::bitcast<UT>(b);

    if constexpr (std::is_same_v<Id, div>) {
        if (b == 0) return {};
        if constexpr (false) {}
        else if constexpr (id == div::sdiv) return s / t;
        else if constexpr (id == div::udiv) return u / v;
        else if constexpr (id == div::srem) return s % t;
        else if constexpr (id == div::urem) return u % v;
        else []<bool flag = false>() { static_assert(flag, "missing sub tag"); }();
    } else {
        []<bool flag = false>() { static_assert(flag, "missing tag"); }();
    }
}
// clang-format on

// Note that @p a and @p b are passed by reference as fold also commutes if possible.
template<class Id, Id id> Ref fold(World& world, Ref type, const Def*& a, const Def*& b) {
    if (a->isa<Bot>() || b->isa<Bot>()) return world.bot(type);

    if (auto la = Lit::isa(a)) {
        if (auto lb = Lit::isa(b)) {
            auto size  = Lit::as(Idx::size(a->type()));
            auto width = Idx::size2bitwidth(size);

            Res res;
            switch (width) {
#define CODE(i) \
    case i: res = fold<Id, id, i>(*la, *lb); break;
                THORIN_1_8_16_32_64(CODE)
#undef CODE
                default:
                    // TODO this is super rough but at least better than just bailing out
                    res = fold<Id, id, 64>(*la, *lb);
                    if (res) *res %= size;
            }

            return res ? world.lit(type, *res) : world.bot(type);
        }
    }

    commute(id, a, b);
    return nullptr;
}

template<class Id, nat_t w> Res fold(u64 a) {
    using ST = w2s<w>;
    auto s   = thorin::bitcast<ST>(a);

    if constexpr (std::is_same_v<Id, abs>) {
        return std::abs(s);
    } else {
        []<bool flag = false>() { static_assert(flag, "missing tag"); }
        ();
    }
}

template<class Id> Ref fold(World& world, Ref type, const Def*& a) {
    if (a->isa<Bot>()) return world.bot(type);

    if (auto la = Lit::isa(a)) {
        auto size  = Lit::as(Idx::size(a->type()));
        auto width = Idx::size2bitwidth(size);
        Res res;
        switch (width) {
#define CODE(i) \
    case i: res = fold<Id, i>(*la); break;
            THORIN_1_8_16_32_64(CODE)
#undef CODE
            default:
                res = fold<Id, 64>(*la);
                if (res) *res %= size;
        }

        return res ? world.lit(type, *res) : world.bot(type);
    }
    return nullptr;
}

} // namespace

Ref normalize_lea(Ref type, Ref callee, Ref arg) {
    auto& world                = type->world();
    auto [ptr, index]          = arg->projs<2>();
    auto [pointee, addr_space] = force<Ptr>(ptr->type())->args<2>();

    if (auto a = Lit::isa(pointee->arity()); a && *a == 1) return ptr;
    // TODO

    return world.raw_app(type, callee, {ptr, index});
}

Ref normalize_load(Ref type, Ref callee, Ref arg) {
    auto& world                = type->world();
    auto [mem, ptr]            = arg->projs<2>();
    auto [pointee, addr_space] = force<Ptr>(ptr->type())->args<2>();

    if (ptr->isa<Bot>()) return world.tuple({mem, world.bot(type->as<Sigma>()->op(1))});

    // loading an empty tuple can only result in an empty tuple
    if (auto sigma = pointee->isa<Sigma>(); sigma && sigma->num_ops() == 0)
        return world.tuple({mem, world.tuple(sigma->type(), {})});

    return world.raw_app(type, callee, {mem, ptr});
}

Ref normalize_remem(Ref type, Ref callee, Ref mem) {
    auto& world = type->world();

    // if (auto m = match<remem>(mem)) mem = m;
    return world.raw_app(type, callee, mem);
}

Ref normalize_store(Ref type, Ref callee, Ref arg) {
    auto& world          = type->world();
    auto [mem, ptr, val] = arg->projs<3>();

    if (ptr->isa<Bot>() || val->isa<Bot>()) return mem;
    if (auto pack = val->isa<Pack>(); pack && pack->body()->isa<Bot>()) return mem;
    if (auto tuple = val->isa<Tuple>()) {
        if (std::ranges::all_of(tuple->ops(), [](Ref op) { return op->isa<Bot>(); })) return mem;
    }

    return world.raw_app(type, callee, {mem, ptr, val});
}

template<div id> Ref normalize_div(Ref full_type, Ref c, Ref arg) {
    auto& world    = full_type->world();
    auto callee    = c->as<App>();
    auto [mem, ab] = arg->projs<2>();
    auto [a, b]    = ab->projs<2>();
    auto [_, type] = full_type->projs<2>(); // peel off actual type
    auto make_res  = [&, mem = mem](Ref res) { return world.tuple({mem, res}); };

    if (auto result = fold<div, id>(world, type, a, b)) return make_res(result);

    if (auto la = Lit::isa(a)) {
        if (*la == 0) return make_res(a); // 0 / b -> 0 and 0 % b -> 0
    }

    if (auto lb = Lit::isa(b)) {
        if (*lb == 0) return make_res(world.bot(type)); // a / 0 -> ⊥ and a % 0 -> ⊥

        if (*lb == 1) {
            switch (id) {
                case div::sdiv: return make_res(a);                  // a / 1 -> a
                case div::udiv: return make_res(a);                  // a / 1 -> a
                case div::srem: return make_res(world.lit(type, 0)); // a % 1 -> 0
                case div::urem: return make_res(world.lit(type, 0)); // a % 1 -> 0
            }
        }
    }

    if (a == b) {
        switch (id) {
            case div::sdiv: return make_res(world.lit(type, 1)); // a / a -> 1
            case div::udiv: return make_res(world.lit(type, 1)); // a / a -> 1
            case div::srem: return make_res(world.lit(type, 0)); // a % a -> 0
            case div::urem: return make_res(world.lit(type, 0)); // a % a -> 0
        }
    }

    return world.raw_app(full_type, callee, arg);
}

Ref normalize_abs(Ref full_type, Ref c, Ref arg) {
    auto& world    = full_type->world();
    auto callee    = c->as<App>();
    auto [mem, a]  = arg->projs<2>();
    auto [_, type] = full_type->projs<2>();

    if (auto result = fold<abs>(world, type, a)) return world.tuple({mem, result});
    return world.raw_app(type, callee, arg);
}

THORIN_mem_NORMALIZER_IMPL

} // namespace thorin::plug::mem
