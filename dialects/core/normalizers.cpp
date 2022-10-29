#include "thorin/normalize.h"

#include "dialects/core/core.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::normalize {

// clang-format off
constexpr bool is_commutative(core::nop    ) { return true; }
constexpr bool is_commutative(core::wrap id) { return id == core::wrap::add || id == core::wrap::mul; }
constexpr bool is_commutative(core::ncmp id) { return id == core::ncmp::  e || id == core::ncmp:: ne; }
constexpr bool is_commutative(core::icmp id) { return id == core::icmp::  e || id == core::icmp:: ne; }
// clang-format off

constexpr bool is_associative(core::bit2 id) {
    switch (id) {
        case core::bit2::t:
        case core::bit2::_xor:
        case core::bit2::_and:
        case core::bit2::nxor:
        case core::bit2::a:
        case core::bit2::b:
        case core::bit2::_or:
        case core::bit2::f: return true;
        default: return false;
    }
}

}

using namespace thorin::normalize;

namespace thorin::core {

template<class T>
static T get(u64 u) {
    return thorin::bitcast<T>(u);
}

// clang-format off
template<class Id> constexpr bool is_int           () { return true;  }
template<>         constexpr bool is_int<math::cmp>() { return false; }
// clang-format on

/*
 * Fold
 */

template<class T, T, nat_t>
struct Fold {};

/// @attention Note that @p a and @p b are passed by reference as fold also commutes if possible. @sa commute().
template<class Id, Id id, bool isa_wrap = std::is_same_v<Id, wrap>>
static const Def* fold(World& world, const Def* type, const App* callee, const Def*& a, const Def*& b, const Def* dbg) {
    auto la = a->isa<Lit>(), lb = b->isa<Lit>();

    if (a->isa<Bot>() || b->isa<Bot>()) return world.bot(type, dbg);

    if (la && lb) {
        nat_t width;
        [[maybe_unused]] bool nsw = false, nuw = false;
        if constexpr (std::is_same_v<Id, wrap>) {
            auto [mode, w] = callee->args<2>(as_lit<nat_t>);
            nsw            = mode & WMode::nsw;
            nuw            = mode & WMode::nuw;
            width          = w;
        } else if (auto size = Idx::size(a->type())) {
            width = as_lit(size);
        }

        width = *size2bitwidth(width);

        Res res;
        switch (width) {
#define CODE(i)                                                             \
    case i:                                                                 \
        if constexpr (isa_wrap)                                         \
            res = Fold<Id, id, i>::run(la->get(), lb->get(), nsw, nuw); \
        else                                                            \
            res = Fold<Id, id, i>::run(la->get(), lb->get());           \
        break;
            THORIN_1_8_16_32_64(CODE)
#undef CODE
            default: unreachable();
        }

        return res ? world.lit(type, *res, dbg) : world.bot(type, dbg);
    }

    commute(id, a, b);
    return nullptr;
}

template<nat_t w>
struct Fold<wrap, wrap::add, w> {
    static Res run(u64 a, u64 b, bool /*nsw*/, bool nuw) {
        auto x = get<w2u<w>>(a), y = get<w2u<w>>(b);
        decltype(x) res = x + y;
        if (nuw && res < x) return {};
        // TODO nsw
        return res;
    }
};

template<nat_t w>
struct Fold<wrap, wrap::sub, w> {
    static Res run(u64 a, u64 b, bool /*nsw*/, bool /*nuw*/) {
        using UT = w2u<w>;
        auto x = get<UT>(a), y = get<UT>(b);
        decltype(x) res = x - y;
        // if (nuw && y && x > std::numeric_limits<UT>::max() / y) return {};
        //  TODO nsw
        return res;
    }
};

template<nat_t w>
struct Fold<wrap, wrap::mul, w> {
    static Res run(u64 a, u64 b, bool /*nsw*/, bool /*nuw*/) {
        using UT = w2u<w>;
        auto x = get<UT>(a), y = get<UT>(b);
        if constexpr (std::is_same_v<UT, bool>)
            return UT(x & y);
        else
            return UT(x * y);
        // TODO nsw/nuw
    }
};

template<nat_t w>
struct Fold<wrap, wrap::shl, w> {
    static Res run(u64 a, u64 b, bool nsw, bool nuw) {
        using T = w2u<w>;
        auto x = get<T>(a), y = get<T>(b);
        if (u64(y) >= w) return {};
        decltype(x) res;
        if constexpr (std::is_same_v<T, bool>)
            res = bool(u64(x) << u64(y));
        else
            res = x << y;
        if (nuw && res < x) return {};
        if (nsw && get_sign(x) != get_sign(res)) return {};
        return res;
    }
};

// clang-format off
template<nat_t w> struct Fold<div, div::sdiv, w> { static Res run(u64 a, u64 b) { using T = w2s<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) / r); } };
template<nat_t w> struct Fold<div, div::udiv, w> { static Res run(u64 a, u64 b) { using T = w2u<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) / r); } };
template<nat_t w> struct Fold<div, div::srem, w> { static Res run(u64 a, u64 b) { using T = w2s<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) % r); } };
template<nat_t w> struct Fold<div, div::urem, w> { static Res run(u64 a, u64 b) { using T = w2u<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) % r); } };

template<nat_t w> struct Fold<shr, shr::a   , w> { static Res run(u64 a, u64 b) { using T = w2s<w>; if (b > w) return {}; return T(get<T>(a) >> get<T>(b)); } };
template<nat_t w> struct Fold<shr, shr::l   , w> { static Res run(u64 a, u64 b) { using T = w2u<w>; if (b > w) return {}; return T(get<T>(a) >> get<T>(b)); } };
// clang-format on

template<icmp cmp, nat_t w>
struct Fold<icmp, cmp, w> {
    inline static Res run(u64 a, u64 b) {
        using T = w2u<w>;
        auto x = get<T>(a), y = get<T>(b);
        bool result = false;
        auto pm     = !(x >> T(w - 1)) && (y >> T(w - 1));
        auto mp     = (x >> T(w - 1)) && !(y >> T(w - 1));
        result |= ((cmp & icmp::Xygle) != icmp::f) && pm;
        result |= ((cmp & icmp::xYgle) != icmp::f) && mp;
        result |= ((cmp & icmp::xyGle) != icmp::f) && x > y && !mp;
        result |= ((cmp & icmp::xygLe) != icmp::f) && x < y && !pm;
        result |= ((cmp & icmp::xyglE) != icmp::f) && x == y;
        return result;
    }
};

// clang-format off
template<conv id, nat_t, nat_t> struct FoldConv {};
template<nat_t ss, nat_t ds> struct FoldConv<conv::s2s, ss, ds> { static Res run(u64 src) { return w2s<ds>(get<w2s<ss>>(src)); } };
template<nat_t ss, nat_t ds> struct FoldConv<conv::u2u, ss, ds> { static Res run(u64 src) { return w2u<ds>(get<w2u<ss>>(src)); } };
// clang-format on

/// Reassociates @p a und @p b according to following rules.
/// We use the following naming convention while literals are prefixed with an 'l':
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
static const Def*
reassociate(Id id, World& /*world*/, [[maybe_unused]] const App* ab, const Def* a, const Def* b, const Def* dbg) {
    if (!is_associative(id)) return nullptr;

    auto la = a->isa<Lit>();
    auto xy = match<Id>(id, a);
    auto zw = match<Id>(id, b);
    auto lx = xy ? xy->arg(0)->template isa<Lit>() : nullptr;
    auto lz = zw ? zw->arg(0)->template isa<Lit>() : nullptr;
    auto y  = xy ? xy->arg(1) : nullptr;
    auto w  = zw ? zw->arg(1) : nullptr;

    std::function<const Def*(const Def*, const Def*)> make_op;

    if constexpr (std::is_same_v<Id, wrap>) {
        // if we reassociate Wraps, we have to forget about nsw/nuw
        make_op = [&](const Def* a, const Def* b) { return op(id, WMode::none, a, b, dbg); };
    } else {
        make_op = [&](const Def* a, const Def* b) { return op(id, a, b, dbg); };
    }

    if (la && lz) return make_op(make_op(la, lz), w);             // (1)
    if (lx && lz) return make_op(make_op(lx, lz), make_op(y, w)); // (2)
    if (lz) return make_op(lz, make_op(a, w));                    // (3)
    if (lx) return make_op(lx, make_op(y, b));                    // (4)

    return nullptr;
}

template<nop id>
const Def* normalize_nop(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto [a, b] = arg->projs<2>();
    commute(id, a, b);

    if (auto la = isa_lit(a)) {
        if (auto lb = isa_lit(b)) {
            switch (id) {
                case nop::add: return world.lit_nat(*la + *lb);
                case nop::mul: return world.lit_nat(*la * *lb);
            }
        }
    }

    return world.raw_app(callee, arg, dbg);
}

template<ncmp id>
const Def* normalize_ncmp(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    if (id == ncmp::t) return world.lit_tt();
    if (id == ncmp::f) return world.lit_ff();

    auto [a, b] = arg->projs<2>();
    commute(id, a, b);

    if (auto la = isa_lit(a)) {
        if (auto lb = isa_lit(b)) {
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

    return world.raw_app(callee, arg, dbg);
}

template<icmp id>
const Def* normalize_icmp(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();

    if (auto result = fold<icmp, id>(world, type, callee, a, b, dbg)) return result;
    if (id == icmp::f) return world.lit_ff();
    if (id == icmp::t) return world.lit_tt();
    if (a == b) {
        if (id == icmp::e) return world.lit_tt();
        if (id == icmp::ne) return world.lit_ff();
    }

    return world.raw_app(callee, {a, b}, dbg);
}

template<bit1 id>
const Def* normalize_bit1(const Def* type, const Def* c, const Def* a, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto s      = isa_lit(callee->arg());
    auto l      = isa_lit(a);

    switch (id) {
        case bit1::f: return world.lit_idx(*s, 0);
        case bit1::t: return world.lit_idx(*s, *s - 1_u64);
        case bit1::id: return a;
        default: break;
    }

    if (l) return world.lit_idx_mod(*s, ~*l);

    return world.raw_app(callee, a, dbg);
}

template<class Id>
static const Def* merge_cmps(std::array<std::array<u64, 2>, 2> tab, const Def* a, const Def* b, const Def* dbg) {
    static_assert(sizeof(sub_t) == 1, "if this ever changes, please adjust the logic below");
    static constexpr size_t num_bits = std::bit_width(Axiom::Num<Id> - 1_u64);

    auto a_cmp = match<Id>(a);
    auto b_cmp = match<Id>(b);

    if (a_cmp && b_cmp && a_cmp->arg() == b_cmp->arg()) {
        // push sub bits of a_cmp and b_cmp through truth table
        sub_t res   = 0;
        sub_t a_sub = a_cmp.sub();
        sub_t b_sub = b_cmp.sub();
        for (size_t i = 0; i != num_bits; ++i, res >>= 1, a_sub >>= 1, b_sub >>= 1)
            res |= tab[a_sub & 1][b_sub & 1] << 7_u8;
        res >>= (7_u8 - u8(num_bits));

        if constexpr (std::is_same_v<Id, math::cmp>)
            return op(math::cmp(res), /*rmode*/ a_cmp->decurry()->arg(0), a_cmp->arg(0), a_cmp->arg(1), dbg);
        else
            return op(icmp(Axiom::Base<icmp> | res), a_cmp->arg(0), a_cmp->arg(1), dbg);
    }

    return nullptr;
}

template<bit2 id>
const Def* normalize_bit2(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto s      = isa_lit(callee->arg());

    commute(id, a, b);

    auto tab = make_truth_table(id);
    if (auto res = merge_cmps<icmp>(tab, a, b, dbg)) return res;
    if (auto res = merge_cmps<math::cmp>(tab, a, b, dbg)) return res;

    auto la = isa_lit(a);
    auto lb = isa_lit(b);

    // clang-format off
    switch (id) {
        case bit2::    f: return world.lit(type,        0);
        case bit2::    t: if (s) return world.lit(type, *s-1_u64); break;
        case bit2::    a: return a;
        case bit2::    b: return b;
        case bit2::   na: return op_negate(a, dbg);
        case bit2::   nb: return op_negate(b, dbg);
        case bit2:: ciff: return op(bit2:: iff, b, a, dbg);
        case bit2::nciff: return op(bit2::niff, b, a, dbg);
        default:         break;
    }

    if (la && lb) {
        switch (id) {
            case bit2::_and: return world.lit_idx    (*s,   *la &  *lb);
            case bit2:: _or: return world.lit_idx    (*s,   *la |  *lb);
            case bit2::_xor: return world.lit_idx    (*s,   *la ^  *lb);
            case bit2::nand: return world.lit_idx_mod(*s, ~(*la &  *lb));
            case bit2:: nor: return world.lit_idx_mod(*s, ~(*la |  *lb));
            case bit2::nxor: return world.lit_idx_mod(*s, ~(*la ^  *lb));
            case bit2:: iff: return world.lit_idx_mod(*s, ~ *la |  *lb);
            case bit2::niff: return world.lit_idx    (*s,   *la & ~*lb);
            default: unreachable();
        }
    }

    auto unary = [&](bool x, bool y, const Def* a) -> const Def* {
        if (!x && !y) return world.lit(type, 0);
        if ( x &&  y) return s ? world.lit(type, *s-1_u64) : nullptr;
        if (!x &&  y) return a;
        if ( x && !y && id != bit2::_xor) return op_negate(a, dbg);
        return nullptr;
    };
    // clang-format on

    if (is_commutative(id) && a == b) {
        if (auto res = unary(tab[0][0], tab[1][1], a)) return res;
    }

    if (la) {
        if (*la == 0) {
            if (auto res = unary(tab[0][0], tab[0][1], b)) return res;
        } else if (*la == *s - 1_u64) {
            if (auto res = unary(tab[1][0], tab[1][1], b)) return res;
        }
    }

    if (lb) {
        if (*lb == 0) {
            if (auto res = unary(tab[0][0], tab[1][0], b)) return res;
        } else if (*lb == *s - 1_u64) {
            if (auto res = unary(tab[0][1], tab[1][1], b)) return res;
        }
    }

    if (auto res = reassociate<bit2>(id, world, callee, a, b, dbg)) return res;

    return world.raw_app(callee, {a, b}, dbg);
}

template<shr id>
const Def* normalize_shr(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto w      = isa_lit(callee->arg());

    if (auto result = fold<shr, id>(world, type, callee, a, b, dbg)) return result;

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

        if (lb->get() > *w) return world.bot(type, dbg);
    }

    return world.raw_app(callee, {a, b}, dbg);
}

template<wrap id>
const Def* normalize_wrap(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto [m, w] = callee->args<2>(isa_lit<nat_t>); // mode and width

    if (auto result = fold<wrap, id, true>(world, type, callee, a, b, dbg)) return result;

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
            return op(wrap::add, *m, a, world.lit_idx_mod(*w, ~lb->get() + 1_u64)); // a - lb -> a + (~lb + 1)
        else if (id == wrap::shl && lb->get() > *w)
            return world.bot(type, dbg);
    }

    if (a == b) {
        switch (id) {
            case wrap::add: return op(wrap::mul, *m, world.lit(type, 2), a, dbg); // a + a -> 2 * a
            case wrap::sub: return world.lit(type, 0);                            // a - a -> 0
            case wrap::mul: break;
            case wrap::shl: break;
        }
    }
    // clang-format on

    if (auto res = reassociate<wrap>(id, world, callee, a, b, dbg)) return res;

    return world.raw_app(callee, {a, b}, dbg);
}

template<div id>
const Def* normalize_div(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world      = type->world();
    auto callee      = c->as<App>();
    auto [mem, a, b] = arg->projs<3>();
    type             = type->as<Sigma>()->op(1); // peel off actual type
    auto make_res    = [&, mem = mem](const Def* res) { return world.tuple({mem, res}, dbg); };

    if (auto result = fold<div, id>(world, type, callee, a, b, dbg)) return make_res(result);

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

    return world.raw_app(callee, {mem, a, b}, dbg);
}

// clang-format off
#define TABLE(m) m( 1,  1) m( 1,  8) m( 1, 16) m( 1, 32) m( 1, 64) \
                 m( 8,  1) m( 8,  8) m( 8, 16) m( 8, 32) m( 8, 64) \
                 m(16,  1) m(16,  8) m(16, 16) m(16, 32) m(16, 64) \
                 m(32,  1) m(32,  8) m(32, 16) m(32, 32) m(32, 64) \
                 m(64,  1) m(64,  8) m(64, 16) m(64, 32) m(64, 64)
// clang-format on

template<conv id>
static const Def* fold_conv(const Def* dst_t, const Def* src, const Def* dbg) {
    auto& world = dst_t->world();
    if (src->isa<Bot>()) return world.bot(dst_t, dbg);

    auto lds = isa_lit(Idx::size(src->type()));
    auto lss = isa_lit(Idx::size(dst_t));
    auto lit = src->isa<Lit>();

    if (lit && lds && lss) {
        if (id == conv::u2u) {
            if (*lds == 0) return world.lit(dst_t, lit->get()); // I64
            return world.lit(dst_t, lit->get() % *lds);
        }

        if (Idx::size(src->type())) *lss = *size2bitwidth(*lss);
        if (Idx::size(dst_t)) *lds = *size2bitwidth(*lds);

        Res res;
        if (false) {}
#define CODE(ss, ds) else if (*lds == ds && *lss == ss) res = FoldConv<id, ds, ss>::run(lit->get());
        TABLE(CODE)
#undef CODE
        if (res) return world.lit(dst_t, *res, dbg);
        return world.bot(dst_t, dbg);
    }

    return nullptr;
}

template<conv id>
const Def* normalize_conv(const Def* dst_t, const Def* c, const Def* src, const Def* dbg) {
    auto& world = dst_t->world();
    auto callee = c->as<App>();

    if (auto result = fold_conv<id>(dst_t, src, dbg)) return result;

    auto ds = isa_lit(callee->decurry()->arg());
    auto ss = isa_lit(callee->decurry()->decurry()->arg());
    if (ss == ds && dst_t == src->type()) return src;

    if constexpr (id == conv::s2s) {
        if (ss && ds && *ss < *ds) return op(conv::u2u, dst_t, src, dbg);
    }

    return world.raw_app(callee, src, dbg);
}

const Def* normalize_bitcast(const Def* dst_t, const Def* callee, const Def* src, const Def* dbg) {
    auto& world = dst_t->world();

    if (src->isa<Bot>()) return world.bot(dst_t);
    if (src->type() == dst_t) return src;

    if (auto other = match<bitcast>(src))
        return other->arg()->type() == dst_t ? other->arg() : op_bitcast(dst_t, other->arg(), dbg);

    if (auto lit = src->isa<Lit>()) {
        if (dst_t->isa<Nat>()) return world.lit(dst_t, lit->get(), dbg);
        if (Idx::size(dst_t)) return world.lit(dst_t, lit->get(), dbg);
    }

    return world.raw_app(callee, src, dbg);
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
const Def* normalize_trait(const Def*, const Def* callee, const Def* type, const Def* dbg) {
    auto& world = type->world();
    if (auto ptr = match<mem::Ptr>(type)) {
        return world.lit_nat(8);
    } else if (type->isa<Pi>()) {
        return world.lit_nat(8); // Gets lowered to function ptr
    } else if (auto size = Idx::size(type)) {
        if (size->isa<Top>()) return world.lit_nat(8);
        if (auto w = isa_lit(size)) {
            if (*w == 0) return world.lit_nat(8);
            if (*w <= 0x0000'0000'0000'0100_u64) return world.lit_nat(1);
            if (*w <= 0x0000'0000'0001'0000_u64) return world.lit_nat(2);
            if (*w <= 0x0000'0001'0000'0000_u64) return world.lit_nat(4);
            return world.lit_nat(8);
        }
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
            auto a = isa_lit(core::op(trait::align, t));
            auto s = isa_lit(core::op(trait::size, t));
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
    } else if (auto arr = type->isa_structural<Arr>()) {
        auto align = op(trait::align, arr->body());
        if constexpr (id == trait::align) return align;
        if (auto b = op(trait::size, arr->body())->isa<Lit>()) return core::op(core::nop::mul, arr->shape(), b);
    } else if (auto join = type->isa<Join>()) {
        if (auto sigma = convert(join)) return core::op(id, sigma, dbg);
    }

out:
    return world.raw_app(callee, type, dbg);
}

const Def* normalize_zip(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& w                    = type->world();
    auto callee                = c->as<App>();
    auto is_os                 = callee->arg();
    auto [n_i, Is, n_o, Os, f] = is_os->projs<5>();
    auto [r, s]                = callee->decurry()->args<2>();
    auto lr                    = isa_lit(r);
    auto ls                    = isa_lit(s);

    // TODO commute
    // TODO reassociate
    // TODO more than one Os
    // TODO select which Is/Os to zip

    if (lr && ls && *lr == 1 && *ls == 1) return w.app(f, arg, dbg);

    if (auto l_in = isa_lit(n_i)) {
        auto args = arg->projs(*l_in);

        if (lr && std::ranges::all_of(args, [](auto arg) { return is_tuple_or_pack(arg); })) {
            auto shapes = s->projs(*lr);
            auto s_n    = isa_lit(shapes.front());

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

    return w.raw_app(callee, arg, dbg);
}

template<pe id>
const Def* normalize_pe(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    if constexpr (id == pe::known) {
        if (match(pe::hlt, arg)) return world.lit_ff();
        if (arg->dep_const()) return world.lit_tt();
    }

    return world.raw_app(callee, arg, dbg);
}

THORIN_core_NORMALIZER_IMPL

} // namespace thorin::core
