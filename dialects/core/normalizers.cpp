#include "dialects/core/core.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::core {

// TODO move to normalize.h
/// Swap Lit to left - or smaller gid, if no lit present.
template<class Id>
void commute(Id id, const Def*& a, const Def*& b) {
    if (is_commutative(id)) {
        if (b->isa<Lit>() || (a->gid() > b->gid() && !a->isa<Lit>())) std::swap(a, b);
    }
}

namespace {
/*
 * Fold
 */

class Res {
public:
    Res()
        : data_{} {}
    template<class T>
    Res(T val)
        : data_(thorin::bitcast<u64>(val)) {}

    constexpr const u64& operator*() const& { return *data_; }
    constexpr u64& operator*() & { return *data_; }
    explicit operator bool() const { return data_.has_value(); }

private:
    std::optional<u64> data_;
};

// clang-format off
// See https://stackoverflow.com/a/64354296 for static_assert trick below.
template<class Id, Id id, nat_t w>
Res fold(u64 a, u64 b, [[maybe_unused]] bool nsw, [[maybe_unused]] bool nuw) {
    using ST = w2s<w>;
    using UT = w2u<w>;
    auto s = thorin::bitcast<ST>(a), t = thorin::bitcast<ST>(b);
    auto u = thorin::bitcast<UT>(a), v = thorin::bitcast<UT>(b);

    if constexpr (std::is_same_v<Id, wrap>) {
        if constexpr (id == wrap::add) {
            auto res = u + v;
            if (nuw && res < u) return {};
            // TODO nsw
            return res;
        } else if constexpr (id == wrap::sub) {
            auto res = u - v;
            //  TODO nsw
            return res;
        } else if constexpr (id == wrap::mul) {
            if constexpr (std::is_same_v<UT, bool>)
                return UT(u & v);
            else
                return UT(u * v);
        } else if constexpr (id == wrap::shl) {
            if (b >= w) return {};
            decltype(u) res;
            if constexpr (std::is_same_v<UT, bool>)
                res = bool(u64(u) << u64(v));
            else
                res = u << v;
            if (nuw && res < u) return {};
            if (nsw && get_sign(u) != get_sign(res)) return {};
            return res;
        } else {
            []<bool flag = false>() { static_assert(flag, "missing sub tag"); }();
        }
    } else if constexpr (std::is_same_v<Id, shr>) {
        if (b >= w) return {};
        if constexpr (false) {}
        else if constexpr (id == shr::a) return s >> t;
        else if constexpr (id == shr::l) return u >> v;
        else []<bool flag = false>() { static_assert(flag, "missing sub tag"); }();
    } else if constexpr (std::is_same_v<Id, div>) {
        if (b == 0) return {};
        if constexpr (false) {}
        else if constexpr (id == div::sdiv) return s / t;
        else if constexpr (id == div::udiv) return u / v;
        else if constexpr (id == div::srem) return s % t;
        else if constexpr (id == div::urem) return u % v;
        else []<bool flag = false>() { static_assert(flag, "missing sub tag"); }();
    } else if constexpr (std::is_same_v<Id, icmp>) {
        bool res = false;
        auto pm  = !(u >> UT(w - 1)) &&  (v >> UT(w - 1));
        auto mp  =  (u >> UT(w - 1)) && !(v >> UT(w - 1));
        res |= ((id & icmp::Xygle) != icmp::f) && pm;
        res |= ((id & icmp::xYgle) != icmp::f) && mp;
        res |= ((id & icmp::xyGle) != icmp::f) && u > v && !mp;
        res |= ((id & icmp::xygLe) != icmp::f) && u < v && !pm;
        res |= ((id & icmp::xyglE) != icmp::f) && u == v;
        return res;
    } else {
        []<bool flag = false>() { static_assert(flag, "missing tag"); }();
    }
}
// clang-format on

/// @attention Note that @p a and @p b are passed by reference as fold also commutes if possible. @sa commute().
template<class Id, Id id>
Ref fold(World& world, Ref type, const Def*& a, const Def*& b, Ref mode = {}) {
    auto la = a->isa<Lit>(), lb = b->isa<Lit>();

    if (a->isa<Bot>() || b->isa<Bot>()) return world.bot(type);

    if (la && lb) {
        auto size  = Lit::as(Idx::size(a->type()));
        auto width = Idx::size2bitwidth(size);
        bool nsw = false, nuw = false;
        if constexpr (std::is_same_v<Id, wrap>) {
            auto m = mode ? Lit::as(mode) : 0_n;
            nsw    = m & Mode::nsw;
            nuw    = m & Mode::nuw;
        }

        Res res;
        switch (width) {
#define CODE(i) \
    case i: res = fold<Id, id, i>(la->get(), lb->get(), nsw, nuw); break;
            THORIN_1_8_16_32_64(CODE)
#undef CODE
            default: unreachable();
        }

        return res ? world.lit(type, *res) : world.bot(type);
    }

    commute(id, a, b);
    return nullptr;
}

/// Reassociates @p a und @p b according to following rules.
/// We use the following naming convention while literals are prefixed with an `l`:
/// ```
///     a    op     b
/// (x op y) op (z op w)
///
/// (1)     la    op (lz op w) -> (la op lz) op w
/// (2) (lx op y) op (lz op w) -> (lx op lz) op (y op w)
/// (3)      a    op (lz op w) ->  lz op (a op w)
/// (4) (lx op y) op      b    ->  lx op (y op b)
/// ```
template<class Id>
Ref reassociate(Id id, World& world, [[maybe_unused]] const App* ab, Ref a, Ref b) {
    if (!is_associative(id)) return nullptr;

    auto la = a->isa<Lit>();
    auto xy = match<Id>(id, a);
    auto zw = match<Id>(id, b);
    auto lx = xy ? xy->arg(0)->template isa<Lit>() : nullptr;
    auto lz = zw ? zw->arg(0)->template isa<Lit>() : nullptr;
    auto y  = xy ? xy->arg(1) : nullptr;
    auto w  = zw ? zw->arg(1) : nullptr;

    // if we reassociate, we have to forget about nsw/nuw
    auto make_op = [&world, id](Ref a, Ref b) { return world.call(id, Mode::none, Defs{a, b}); };

    if (la && lz) return make_op(make_op(la, lz), w);             // (1)
    if (lx && lz) return make_op(make_op(lx, lz), make_op(y, w)); // (2)
    if (lz) return make_op(lz, make_op(a, w));                    // (3)
    if (lx) return make_op(lx, make_op(y, b));                    // (4)

    return nullptr;
}

template<class Id>
Ref merge_cmps(std::array<std::array<u64, 2>, 2> tab, Ref a, Ref b) {
    static_assert(sizeof(sub_t) == 1, "if this ever changes, please adjust the logic below");
    static constexpr size_t num_bits = std::bit_width(Axiom::Num<Id> - 1_u64);

    auto& world = a->world();
    auto a_cmp  = match<Id>(a);
    auto b_cmp  = match<Id>(b);

    if (a_cmp && b_cmp && a_cmp->arg() == b_cmp->arg()) {
        // push sub bits of a_cmp and b_cmp through truth table
        sub_t res   = 0;
        sub_t a_sub = a_cmp.sub();
        sub_t b_sub = b_cmp.sub();
        for (size_t i = 0; i != num_bits; ++i, res >>= 1, a_sub >>= 1, b_sub >>= 1)
            res |= tab[a_sub & 1][b_sub & 1] << 7_u8;
        res >>= (7_u8 - u8(num_bits));

        if constexpr (std::is_same_v<Id, math::cmp>)
            return world.call(math::cmp(res), /*rmode*/ a_cmp->decurry()->arg(0), Defs{a_cmp->arg(0), a_cmp->arg(1)});
        else
            return world.call(icmp(Axiom::Base<icmp> | res), Defs{a_cmp->arg(0), a_cmp->arg(1)});
    }

    return nullptr;
}
} // namespace

template<nat id>
Ref normalize_nat(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    auto [a, b] = arg->projs<2>();
    commute(id, a, b);

    if (auto la = Lit::isa(a)) {
        if (auto lb = Lit::isa(b)) {
            switch (id) {
                case nat::add: return world.lit_nat(*la + *lb);
                case nat::mul: return world.lit_nat(*la * *lb);
            }
        }
    }

    return world.raw_app(type, callee, arg);
}

template<ncmp id>
Ref normalize_ncmp(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();

    if (id == ncmp::t) return world.lit_tt();
    if (id == ncmp::f) return world.lit_ff();

    auto [a, b] = arg->projs<2>();
    commute(id, a, b);

    if (auto la = Lit::isa(a)) {
        if (auto lb = Lit::isa(b)) {
            // clang-format off
            switch (id) {
                case ncmp:: e: return world.lit_nat(*la == *lb);
                case ncmp::ne: return world.lit_nat(*la != *lb);
                case ncmp::l : return world.lit_nat(*la <  *lb);
                case ncmp::le: return world.lit_nat(*la <= *lb);
                case ncmp::g : return world.lit_nat(*la >  *lb);
                case ncmp::ge: return world.lit_nat(*la >= *lb);
                default: unreachable();
            }
            // clang-format on
        }
    }

    return world.raw_app(type, callee, arg);
}

template<icmp id>
Ref normalize_icmp(Ref type, Ref c, Ref arg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();

    if (auto result = fold<icmp, id>(world, type, a, b)) return result;
    if (id == icmp::f) return world.lit_ff();
    if (id == icmp::t) return world.lit_tt();
    if (a == b) {
        if (id == icmp::e) return world.lit_tt();
        if (id == icmp::ne) return world.lit_ff();
    }

    return world.raw_app(type, callee, {a, b});
}

template<bit1 id>
Ref normalize_bit1(Ref type, Ref c, Ref a) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto s      = callee->decurry()->arg();
    // TODO cope with wrap around

    if constexpr (id == bit1::id) return a;

    if (auto ls = Lit::isa(s)) {
        switch (id) {
            case bit1::f: return world.lit_idx(*ls, 0);
            case bit1::t: return world.lit_idx(*ls, *ls - 1_u64);
            case bit1::id: unreachable();
            default: break;
        }

        assert(id == bit1::neg);
        if (auto la = Lit::isa(a)) return world.lit_idx_mod(*ls, ~*la);
    }

    return world.raw_app(type, callee, a);
}

template<bit2 id>
Ref normalize_bit2(Ref type, Ref c, Ref arg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto s      = callee->decurry()->arg();
    auto ls     = Lit::isa(s);
    // TODO cope with wrap around

    commute(id, a, b);

    auto tab = make_truth_table(id);
    if (auto res = merge_cmps<icmp>(tab, a, b)) return res;
    if (auto res = merge_cmps<math::cmp>(tab, a, b)) return res;

    auto la = Lit::isa(a);
    auto lb = Lit::isa(b);

    // clang-format off
    switch (id) {
        case bit2::    f: return world.lit(type, 0);
        case bit2::    t: if (ls) return world.lit(type, *ls-1_u64); break;
        case bit2::  fst: return a;
        case bit2::  snd: return b;
        case bit2:: nfst: return world.call(bit1::neg,  s, a);
        case bit2:: nsnd: return world.call(bit1::neg,  s, b);
        case bit2:: ciff: return world.call(bit2:: iff, s, Defs{b, a});
        case bit2::nciff: return world.call(bit2::niff, s, Defs{b, a});
        default:         break;
    }

    if (la && lb && ls) {
        switch (id) {
            case bit2::and_: return world.lit_idx    (*ls,   *la &  *lb);
            case bit2:: or_: return world.lit_idx    (*ls,   *la |  *lb);
            case bit2::xor_: return world.lit_idx    (*ls,   *la ^  *lb);
            case bit2::nand: return world.lit_idx_mod(*ls, ~(*la &  *lb));
            case bit2:: nor: return world.lit_idx_mod(*ls, ~(*la |  *lb));
            case bit2::nxor: return world.lit_idx_mod(*ls, ~(*la ^  *lb));
            case bit2:: iff: return world.lit_idx_mod(*ls, ~ *la |  *lb);
            case bit2::niff: return world.lit_idx    (*ls,   *la & ~*lb);
            default: unreachable();
        }
    }

    // TODO rewrite using bit2
    auto unary = [&](bool x, bool y, Ref a) -> Ref {
        if (!x && !y) return world.lit(type, 0);
        if ( x &&  y) return ls ? world.lit(type, *ls-1_u64) : nullptr;
        if (!x &&  y) return a;
        if ( x && !y && id != bit2::xor_) return world.call(bit1::neg, s, a);
        return nullptr;
    };
    // clang-format on

    if (is_commutative(id) && a == b) {
        if (auto res = unary(tab[0][0], tab[1][1], a)) return res;
    }

    if (la) {
        if (*la == 0) {
            if (auto res = unary(tab[0][0], tab[0][1], b)) return res;
        } else if (ls && *la == *ls - 1_u64) {
            if (auto res = unary(tab[1][0], tab[1][1], b)) return res;
        }
    }

    if (lb) {
        if (*lb == 0) {
            if (auto res = unary(tab[0][0], tab[1][0], a)) return res;
        } else if (ls && *lb == *ls - 1_u64) {
            if (auto res = unary(tab[0][1], tab[1][1], a)) return res;
        }
    }

    if (auto res = reassociate<bit2>(id, world, callee, a, b)) return res;

    return world.raw_app(type, callee, {a, b});
}

template<shr id>
Ref normalize_shr(Ref type, Ref c, Ref arg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto s      = Idx::size(arg->type());
    auto ls     = Lit::isa(s);

    if (auto result = fold<shr, id>(world, type, a, b)) return result;

    if (auto la = a->isa<Lit>()) {
        if (la == world.lit(type, 0)) {
            switch (id) {
                case shr::a: return la;
                case shr::l: return la;
            }
        }
    }

    if (auto lb = b->isa<Lit>()) {
        if (lb == world.lit(type, 0)) {
            switch (id) {
                case shr::a: return a;
                case shr::l: return a;
            }
        }

        if (ls && lb->get() > *ls) return world.bot(type);
    }

    return world.raw_app(type, callee, {a, b});
}

template<wrap id>
Ref normalize_wrap(Ref type, Ref c, Ref arg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto mode   = callee->arg();
    auto s      = Idx::size(a->type());
    auto ls     = Lit::isa(s);

    if (auto result = fold<wrap, id>(world, type, a, b)) return result;

    // clang-format off
    if (auto la = a->isa<Lit>()) {
        if (la == world.lit(type, 0)) {
            switch (id) {
                case wrap::add: return b;    // 0  + b -> b
                case wrap::sub: break;
                case wrap::mul: return la;   // 0  * b -> 0
                case wrap::shl: return la;   // 0 << b -> 0
            }
        }

        if (la == world.lit(type, 1)) {
            switch (id) {
                case wrap::add: break;
                case wrap::sub: break;
                case wrap::mul: return b;    // 1  * b -> b
                case wrap::shl: break;
            }
        }
    }

    if (auto lb = b->isa<Lit>()) {
        if (lb == world.lit(type, 0)) {
            switch (id) {
                case wrap::sub: return a;    // a  - 0 -> a
                case wrap::shl: return a;    // a >> 0 -> a
                default: unreachable();
                // add, mul are commutative, the literal has been normalized to the left
            }
        }

        if (id == wrap::sub)
            return world.call(wrap::add, mode, Defs{a, world.lit_idx_mod(*ls, ~lb->get() + 1_u64)}); // a - lb -> a + (~lb + 1)
        else if (id == wrap::shl && ls && lb->get() > *ls)
            return world.bot(type);
    }

    if (a == b) {
        switch (id) {
            case wrap::add: return world.call(wrap::mul, mode, Defs{world.lit(type, 2), a}); // a + a -> 2 * a
            case wrap::sub: return world.lit(type, 0);                                       // a - a -> 0
            case wrap::mul: break;
            case wrap::shl: break;
        }
    }
    // clang-format on

    if (auto res = reassociate<wrap>(id, world, callee, a, b)) return res;

    return world.raw_app(type, callee, {a, b});
}

template<div id>
Ref normalize_div(Ref full_type, Ref c, Ref arg) {
    auto& world      = full_type->world();
    auto callee      = c->as<App>();
    auto [mem, a, b] = arg->projs<3>();
    auto [_, type]   = full_type->projs<2>(); // peel off actual type
    auto make_res    = [&, mem = mem](Ref res) { return world.tuple({mem, res}); };

    if (auto result = fold<div, id>(world, type, a, b)) return make_res(result);

    if (auto la = a->isa<Lit>()) {
        if (la == world.lit(type, 0)) return make_res(la); // 0 / b -> 0 and 0 % b -> 0
    }

    if (auto lb = b->isa<Lit>()) {
        if (lb == world.lit(type, 0)) return make_res(world.bot(type)); // a / 0 -> ⊥ and a % 0 -> ⊥

        if (lb == world.lit(type, 1)) {
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

    return world.raw_app(full_type, callee, {mem, a, b});
}

template<conv id>
Ref normalize_conv(Ref dst_t, Ref c, Ref x) {
    auto& world = dst_t->world();
    auto callee = c->as<App>();
    auto s_t    = x->type()->as<App>();
    auto d_t    = dst_t->as<App>();
    auto s      = s_t->arg();
    auto d      = d_t->arg();
    auto ls     = Lit::isa(s);
    auto ld     = Lit::isa(d);

    if (s_t == d_t) return x;
    if (x->isa<Bot>()) return world.bot(d_t);
    if constexpr (id == conv::s) {
        if (ls && ld && *ld < *ls) return world.call(conv::u, d, x); // just truncate - we don't care for signedness
    }

    if (auto l = Lit::isa(x); l && ls && ld) {
        if constexpr (id == conv::u) {
            if (*ld == 0) return world.lit(d_t, *l); // I64
            return world.lit(d_t, *l % *ld);
        }

        auto sw = Idx::size2bitwidth(*ls);
        auto dw = Idx::size2bitwidth(*ld);

        // clang-format off
        if (false) {}
#define M(S, D) \
        else if (S == sw && D == dw) return world.lit(d_t, w2s<D>(thorin::bitcast<w2s<S>>(*l)));
        M( 1,  8) M( 1, 16) M( 1, 32) M( 1, 64)
                  M( 8, 16) M( 8, 32) M( 8, 64)
                            M(16, 32) M(16, 64)
                                      M(32, 64)
        else assert(false && "TODO: conversion between different Idx sizes");
        // clang-format on
    }

    return world.raw_app(dst_t, callee, x);
}

Ref normalize_bitcast(Ref dst_t, Ref callee, Ref src) {
    auto& world = dst_t->world();
    auto src_t  = src->type();

    if (src->isa<Bot>()) return world.bot(dst_t);
    if (src_t == dst_t) return src;

    if (auto other = match<bitcast>(src))
        return other->arg()->type() == dst_t ? other->arg() : world.call<bitcast>(dst_t, other->arg());

    if (auto lit = src->isa<Lit>()) {
        if (dst_t->isa<Nat>()) return world.lit(dst_t, lit->get());
        if (Idx::size(dst_t)) return world.lit(dst_t, lit->get());
    }

    return world.raw_app(dst_t, callee, src);
}

// TODO I guess we can do that with C++20 <bit>
inline u64 pad(u64 offset, u64 align) {
    auto mod = offset % align;
    if (mod != 0) offset += align - mod;
    return offset;
}

// TODO this currently hard-codes x86_64 ABI
// TODO in contrast to C, we might want to give singleton types like '.Idx 1' or '[]' a size of 0 and simply nuke each
// and every occurance of these types in a later phase
// TODO Pi and others
template<trait id>
Ref normalize_trait(Ref nat, Ref callee, Ref type) {
    auto& world = type->world();
    if (auto ptr = match<mem::Ptr>(type)) {
        return world.lit_nat(8);
    } else if (type->isa<Pi>()) {
        return world.lit_nat(8); // Gets lowered to function ptr
    } else if (auto size = Idx::size(type)) {
        if (auto w = Idx::size2bitwidth(size)) return world.lit_nat(std::max(1_n, std::bit_ceil(*w) / 8_n));
    } else if (auto w = math::isa_f(type)) {
        switch (*w) {
            case 16: return world.lit_nat(2);
            case 32: return world.lit_nat(4);
            case 64: return world.lit_nat(8);
            default: unreachable();
        }
    } else if (type->isa<Sigma>() || type->isa<Meet>()) {
        u64 offset = 0;
        u64 align  = 1;
        for (auto t : type->ops()) {
            auto a = Lit::isa(core::op(trait::align, t));
            auto s = Lit::isa(core::op(trait::size, t));
            if (!a || !s) goto out;

            align  = std::max(align, *a);
            offset = pad(offset, *a) + *s;
        }

        offset   = pad(offset, align);
        u64 size = std::max(1_u64, offset);

        switch (id) {
            case trait::align: return world.lit_nat(align);
            case trait::size: return world.lit_nat(size);
        }
    } else if (auto arr = type->isa_imm<Arr>()) {
        auto align = op(trait::align, arr->body());
        if constexpr (id == trait::align) return align;
        if (auto b = op(trait::size, arr->body())->isa<Lit>()) return world.call(nat::mul, Defs{arr->shape(), b});
    } else if (auto join = type->isa<Join>()) {
        if (auto sigma = convert(join)) return core::op(id, sigma);
    }

out:
    return world.raw_app(nat, callee, type);
}

Ref normalize_zip(Ref type, Ref c, Ref arg) {
    auto& w                    = type->world();
    auto callee                = c->as<App>();
    auto is_os                 = callee->arg();
    auto [n_i, Is, n_o, Os, f] = is_os->projs<5>();
    auto [r, s]                = callee->decurry()->args<2>();
    auto lr                    = Lit::isa(r);
    auto ls                    = Lit::isa(s);

    // TODO commute
    // TODO reassociate
    // TODO more than one Os
    // TODO select which Is/Os to zip

    if (lr && ls && *lr == 1 && *ls == 1) return w.app(f, arg);

    if (auto l_in = Lit::isa(n_i)) {
        auto args = arg->projs(*l_in);

        if (lr && std::ranges::all_of(args, [](Ref arg) { return arg->isa<Tuple, Pack>(); })) {
            auto shapes = s->projs(*lr);
            auto s_n    = Lit::isa(shapes.front());

            if (s_n) {
                DefArray elems(*s_n, [&, f = f](size_t s_i) {
                    DefArray inner_args(args.size(), [&](size_t i) { return args[i]->proj(*s_n, s_i); });
                    if (*lr == 1) {
                        return w.app(f, inner_args);
                    } else {
                        auto app_zip = w.app(w.ax<zip>(), {w.lit_nat(*lr - 1), w.tuple(shapes.skip_front())});
                        return w.app(w.app(app_zip, is_os), inner_args);
                    }
                });
                return w.tuple(elems);
            }
        }
    }

    return w.raw_app(type, callee, arg);
}

template<pe id>
Ref normalize_pe(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();

    if constexpr (id == pe::known) {
        if (match(pe::hlt, arg)) return world.lit_ff();
        if (arg->dep_const()) return world.lit_tt();
    }

    return world.raw_app(type, callee, arg);
}

THORIN_core_NORMALIZER_IMPL

} // namespace thorin::core
