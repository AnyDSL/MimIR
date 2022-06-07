#include <type_traits>

#include "thorin/normalize.h"
#include "thorin/tables.h"

#include "dialects/core.h"
#include "dialects/core/core.h"

namespace thorin::normalize {
using namespace thorin::core;

/// Use like this:
/// `a op b = tab[a][b]`
constexpr std::array<std::array<u64, 2>, 2> make_truth_table(bit2 sub) {
    return {
        {{sub_t(sub) & sub_t(0b0001) ? u64(-1) : 0, sub_t(sub) & sub_t(0b0100) ? u64(-1) : 0},
         {sub_t(sub) & sub_t(0b0010) ? u64(-1) : 0, sub_t(sub) & sub_t(0b1000) ? u64(-1) : 0}}
    };
}

// clang-format off

// we rely on dependent lookup, so these cannot be overloads, but instead have to be
// template specializations
template <>
constexpr bool is_commutative(wrap sub) { return sub == wrap:: add || sub == wrap::mul; }
// todo: real op fixme
// constexpr bool is_commutative(ROp  sub) { return sub == ROp :: add || sub == ROp ::mul; }
template <>
constexpr bool is_commutative(icmp sub) { return sub == icmp::   e || sub == icmp:: ne; }
// todo: real op fixme
// constexpr bool is_commutative(RCmp sub) { return sub == RCmp::   e || sub == RCmp:: ne; }
template <>
constexpr bool is_commutative(bit2  sub) {
    auto tab = make_truth_table(sub);
    return tab[0][1] == tab[1][0];
}
// clang-format off

template<>
constexpr bool is_associative(bit2 sub) {
    switch (sub) {
        case bit2::t:
        case bit2::_xor:
        case bit2::_and:
        case bit2::nxor:
        case bit2::a:
        case bit2::b:
        case bit2::_or:
        case bit2::f: return true;
        default: return false;
    }
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
// todo: div op fixme
// template<nat_t w> struct Fold<Div, Div::sdiv, w> { static Res run(u64 a, u64 b) { using T = w2s<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) / r); } };
// template<nat_t w> struct Fold<Div, Div::udiv, w> { static Res run(u64 a, u64 b) { using T = w2u<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) / r); } };
// template<nat_t w> struct Fold<Div, Div::srem, w> { static Res run(u64 a, u64 b) { using T = w2s<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) % r); } };
// template<nat_t w> struct Fold<Div, Div::urem, w> { static Res run(u64 a, u64 b) { using T = w2u<w>; T r = get<T>(b); if (r == 0) return {}; return T(get<T>(a) % r); } };

template<nat_t w> struct Fold<shr, shr::ashr, w> { static Res run(u64 a, u64 b) { using T = w2s<w>; if (b > w) return {}; return T(get<T>(a) >> get<T>(b)); } };
template<nat_t w> struct Fold<shr, shr::lshr, w> { static Res run(u64 a, u64 b) { using T = w2u<w>; if (b > w) return {}; return T(get<T>(a) >> get<T>(b)); } };

// todo: real op fixme
// template<nat_t w> struct Fold<ROp, ROp:: add, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) + get<T>(b)); } };
// template<nat_t w> struct Fold<ROp, ROp:: sub, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) - get<T>(b)); } };
// template<nat_t w> struct Fold<ROp, ROp:: mul, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) * get<T>(b)); } };
// template<nat_t w> struct Fold<ROp, ROp:: div, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) / get<T>(b)); } };
// template<nat_t w> struct Fold<ROp, ROp:: rem, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(rem(get<T>(a), get<T>(b))); } };
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

template<class AxTag>
static const Def* merge_cmps(std::array<std::array<u64, 2>, 2> tab, const Def* a, const Def* b, const Def* dbg) {
    static_assert(sizeof(sub_t) == 1, "if this ever changes, please adjust the logic below");
    static constexpr size_t num_bits = std::bit_width(Num<AxTag> - 1_u64);

    auto a_cmp = match<AxTag>(a);
    auto b_cmp = match<AxTag>(b);

    if (a_cmp && b_cmp && a_cmp->args() == b_cmp->args()) {
        // push sub bits of a_cmp and b_cmp through truth table
        sub_t res   = 0;
        sub_t a_sub = a_cmp.sub();
        sub_t b_sub = b_cmp.sub();
        for (size_t i = 0; i != num_bits; ++i, res >>= 1, a_sub >>= 1, b_sub >>= 1)
            res |= tab[a_sub & 1][b_sub & 1] << 7_u8;
        res >>= (7_u8 - u8(num_bits));

        // auto& world = a->world();
        // todo: real op fixme
        // if constexpr (std::is_same_v<AxTag, rcmp>)
        // return op(rcmp(res), /*rmode*/ a_cmp->decurry()->arg(0), a_cmp->arg(0), a_cmp->arg(1), dbg);
        // else
        return op(icmp(flags_t(icmp::base_) | res), a_cmp->arg(0), a_cmp->arg(1), dbg);
    }

    return nullptr;
}

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
template<class AxTag>
static const Def*
reassociate(AxTag sub, World& world, [[maybe_unused]] const App* ab, const Def* a, const Def* b, const Def* dbg) {
    if (!is_associative(sub)) return nullptr;

    auto la = a->isa<Lit>();
    auto xy = match<AxTag>(sub, a);
    auto zw = match<AxTag>(sub, b);
    auto lx = xy ? xy->arg(0)->template isa<Lit>() : nullptr;
    auto lz = zw ? zw->arg(0)->template isa<Lit>() : nullptr;
    auto y  = xy ? xy->arg(1) : nullptr;
    auto w  = zw ? zw->arg(1) : nullptr;

    std::function<const Def*(const Def*, const Def*)> make_op;

    // todo: real op fixme
    // if constexpr (sub == Tag::ROp) {
    //     // build rmode for all new ops by using the least upper bound of all involved apps
    //     nat_t rmode     = RMode::bot;
    //     auto check_mode = [&](const App* app) {
    //         auto app_m = isa_lit(app->arg(0));
    //         if (!app_m || !(*app_m & RMode::reassoc)) return false;
    //         rmode &= *app_m; // least upper bound
    //         return true;
    //     };

    //     if (!check_mode(ab)) return nullptr;
    //     if (lx && !check_mode(xy->decurry())) return nullptr;
    //     if (lz && !check_mode(zw->decurry())) return nullptr;

    //     make_op = [&](const Def* a, const Def* b) { return op(sub, rmode, a, b, dbg); };
    // } else
    if constexpr (std::is_same_v<AxTag, wrap>) {
        // if we reassociate Wraps, we have to forget about nsw/nuw
        make_op = [&](const Def* a, const Def* b) { return op(sub, WMode::none, a, b, dbg); };
    } else {
        make_op = [&](const Def* a, const Def* b) { return op(sub, a, b, dbg); };
    }

    if (la && lz) return make_op(make_op(la, lz), w);             // (1)
    if (lx && lz) return make_op(make_op(lx, lz), make_op(y, w)); // (2)
    if (lz) return make_op(lz, make_op(a, w));                    // (3)
    if (lx) return make_op(lx, make_op(y, b));                    // (4)

    return nullptr;
}
} // namespace thorin::normalize

namespace thorin::core {

using namespace thorin::normalize;

template<icmp sub>
const Def* normalize_icmp(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();

    if (auto result = fold<icmp, sub>(world, type, callee, a, b, dbg)) return result;
    if (sub == icmp::f) return world.lit_false();
    if (sub == icmp::t) return world.lit_true();
    if (a == b) {
        if (sub == icmp::e) return world.lit_true();
        if (sub == icmp::ne) return world.lit_false();
    }

    return world.raw_app(callee, {a, b}, dbg);
}

template<bit2 sub>
const Def* normalize_bit(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto w      = isa_lit(callee->arg());

    commute(sub, a, b);

    auto tab = make_truth_table(sub);
    if (auto res = merge_cmps<icmp>(tab, a, b, dbg)) return res;
    // todo: real op fixme
    // if (auto res = merge_cmps<rcmp>(tab, a, b, dbg)) return res;

    auto la = isa_lit(a);
    auto lb = isa_lit(b);

    // clang-format off
    switch (sub) {
        case bit2::    f: return world.lit_int(*w,        0);
        case bit2::    t: return world.lit_int(*w, *w-1_u64);
        case bit2::    a: return a;
        case bit2::    b: return b;
        case bit2::   na: return world.op_negate(a, dbg);
        case bit2::   nb: return world.op_negate(b, dbg);
        case bit2:: ciff: return op(bit2:: iff, b, a, dbg);
        case bit2::nciff: return op(bit2::niff, b, a, dbg);
        default:         break;
    }

    if (la && lb) {
        switch (sub) {
            case bit2::_and: return world.lit_int    (*w,   *la &  *lb);
            case bit2:: _or: return world.lit_int    (*w,   *la |  *lb);
            case bit2::_xor: return world.lit_int    (*w,   *la ^  *lb);
            case bit2::nand: return world.lit_int_mod(*w, ~(*la &  *lb));
            case bit2:: nor: return world.lit_int_mod(*w, ~(*la |  *lb));
            case bit2::nxor: return world.lit_int_mod(*w, ~(*la ^  *lb));
            case bit2:: iff: return world.lit_int_mod(*w, ~ *la |  *lb);
            case bit2::niff: return world.lit_int    (*w,   *la & ~*lb);
            default: unreachable();
        }
    }

    auto unary = [&](bool x, bool y, const Def* a) -> const Def* {
        if (!x && !y) return world.lit_int(*w, 0);
        if ( x &&  y) return world.lit_int(*w, *w-1_u64);
        if (!x &&  y) return a;
        if ( x && !y && sub != bit2::_xor) return world.op_negate(a, dbg);
        return nullptr;
    };
    // clang-format on

    if (is_commutative(sub) && a == b) {
        if (auto res = unary(tab[0][0], tab[1][1], a)) return res;
    }

    if (la) {
        if (*la == 0) {
            if (auto res = unary(tab[0][0], tab[0][1], b)) return res;
        } else if (*la == *w - 1_u64) {
            if (auto res = unary(tab[1][0], tab[1][1], b)) return res;
        }
    }

    if (lb) {
        if (*lb == 0) {
            if (auto res = unary(tab[0][0], tab[1][0], b)) return res;
        } else if (*lb == *w - 1_u64) {
            if (auto res = unary(tab[0][1], tab[1][1], b)) return res;
        }
    }

#if 0
    // TODO
    // commute NOT to b
    if (is_commutative(sub) && is_not(a)) std::swap(a, b);

    if (auto bb = is_not(b); bb && a == bb) {
        switch (sub) {
            case IOp::iand: return world.lit_int(*w,       0);
            case IOp::ior : return world.lit_int(*w, *w-1_u64);
            case IOp::ixor: return world.lit_int(*w, *w-1_u64);
            default: unreachable();
        }
    }

    auto absorption = [&](IOp op1, IOp op2) -> const Def* {
        if (sub == op1) {
            if (auto xy = isa<Tag::IOp>(op2, a)) {
                auto [x, y] = xy->args<2>();
                if (x == b) return y; // (b op2 y) op1 b -> y
                if (y == b) return x; // (x op2 b) op1 b -> x
            }

            if (auto zw = isa<Tag::IOp>(op2, b)) {
                auto [z, w] = zw->args<2>();
                if (z == a) return w; // a op1 (a op2 w) -> w
                if (w == a) return z; // a op1 (z op2 a) -> z
            }
        }
        return nullptr;
    };

    auto simplify1 = [&](IOp op1, IOp op2) -> const Def* { // AFAIK this guy has no name
        if (sub == op1) {
            if (auto xy = isa<Tag::IOp>(op2, a)) {
                auto [x, y] = xy->args<2>();
                if (auto yy = is_not(y); yy && yy == b) return op(op1, x, b, dbg); // (x op2 not b) op1 b -> x op1 y
            }

            if (auto zw = isa<Tag::IOp>(op2, b)) {
                auto [z, w] = zw->args<2>();
                if (auto ww = is_not(w); ww && ww == a) return op(op1, a, z, dbg); // a op1 (z op2 not a) -> a op1 z
            }
        }
        return nullptr;
    };

    auto simplify2 = [&](IOp op1, IOp op2) -> const Def* { // AFAIK this guy has no name
        if (sub == op1) {
            if (auto xy = isa<Tag::IOp>(op2, a)) {
                if (auto zw = isa<Tag::IOp>(op2, b)) {
                    auto [x, y] = xy->args<2>();
                    auto [z, w] = zw->args<2>();
                    if (auto yy = is_not(y); yy && x == z && yy == w) return x; // (z op2 not w) op1 (z op2 w) -> x
                    if (auto ww = is_not(w); ww && x == z && ww == y) return x; // (x op2 y) op1 (x op2 not y) -> x
                }
            }

        }
        return nullptr;
    };

    if (auto res = absorption(IOp::ior , IOp::iand)) return res;
    if (auto res = absorption(IOp::iand, IOp::ior )) return res;
    if (auto res = simplify1 (IOp::ior , IOp::iand)) return res;
    if (auto res = simplify1 (IOp::iand, IOp::ior )) return res;
    if (auto res = simplify2 (IOp::ior , IOp::iand)) return res;
    if (auto res = simplify2 (IOp::iand, IOp::ior )) return res;
#endif
    if (auto res = reassociate<bit2>(sub, world, callee, a, b, dbg)) return res;

    return world.raw_app(callee, {a, b}, dbg);
}

template<shr sub>
const Def* normalize_shr(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto w      = isa_lit(callee->arg());

    if (auto result = fold<shr, sub>(world, type, callee, a, b, dbg)) return result;

    if (auto la = a->isa<Lit>()) {
        if (la == world.lit_int(*w, 0)) {
            switch (sub) {
                case shr::ashr: return la;
                case shr::lshr: return la;
                default: unreachable();
            }
        }
    }

    if (auto lb = b->isa<Lit>()) {
        if (lb == world.lit_int(*w, 0)) {
            switch (sub) {
                case shr::ashr: return a;
                case shr::lshr: return a;
                default: unreachable();
            }
        }

        if (lb->get() > *w) return world.bot(type, dbg);
    }

    return world.raw_app(callee, {a, b}, dbg);
}

template<wrap sub>
const Def* normalize_wrap(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto [m, w] = callee->args<2>(isa_lit<nat_t>); // mode and width

    if (auto result = fold<wrap, sub, true>(world, type, callee, a, b, dbg)) return result;

    // clang-format off
    if (auto la = a->isa<Lit>()) {
        if (la == world.lit_int(*w, 0)) {
            switch (sub) {
                case wrap::add: return b;    // 0  + b -> b
                case wrap::sub: break;
                case wrap::mul: return la;   // 0  * b -> 0
                case wrap::shl: return la;   // 0 << b -> 0
                default: unreachable();
            }
        }

        if (la == world.lit_int(*w, 1)) {
            switch (sub) {
                case wrap::add: break;
                case wrap::sub: break;
                case wrap::mul: return b;    // 1  * b -> b
                case wrap::shl: break;
                default: unreachable();
            }
        }
    }

    if (auto lb = b->isa<Lit>()) {
        if (lb == world.lit_int(*w, 0)) {
            switch (sub) {
                case wrap::sub: return a;    // a  - 0 -> a
                case wrap::shl: return a;    // a >> 0 -> a
                default: unreachable();
                // add, mul are commutative, the literal has been normalized to the left
            }
        }

        if (sub == wrap::sub)
            return op(wrap::add, *m, a, world.lit_int_mod(*w, ~lb->get() + 1_u64)); // a - lb -> a + (~lb + 1)
        else if (sub == wrap::shl && lb->get() > *w)
            return world.bot(type, dbg);
    }

    if (a == b) {
        switch (sub) {
            case wrap::add: return op(wrap::mul, *m, world.lit_int(*w, 2), a, dbg); // a + a -> 2 * a
            case wrap::sub: return world.lit_int(*w, 0);                                  // a - a -> 0
            case wrap::mul: break;
            case wrap::shl: break;
            default: unreachable();
        }
    }
    // clang-format on

    if (auto res = reassociate<wrap>(sub, world, callee, a, b, dbg)) return res;

    return world.raw_app(callee, {a, b}, dbg);
}

THORIN_core_NORMALIZER_IMPL

} // namespace thorin::core
