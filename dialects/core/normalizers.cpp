#include <type_traits>

#include "dialects/core/core.h"
#include "dialects/mem/mem.h"

namespace thorin::core {

template<class T>
static T get(u64 u) {
    return thorin::bitcast<T>(u);
}

// clang-format off
template<class Id> constexpr bool is_int      () { return true;  }
template<>         constexpr bool is_int<rop >() { return false; }
template<>         constexpr bool is_int<rcmp>() { return false; }
// clang-format on

/// Use like this:
/// `a op b = tab[a][b]`
constexpr std::array<std::array<u64, 2>, 2> make_truth_table(bit2 id) {
    return {
        {{sub_t(id) & sub_t(0b0001) ? u64(-1) : 0, sub_t(id) & sub_t(0b0100) ? u64(-1) : 0},
         {sub_t(id) & sub_t(0b0010) ? u64(-1) : 0, sub_t(id) & sub_t(0b1000) ? u64(-1) : 0}}
    };
}

/*
 * is_commutative & is_associative
 */

// clang-format off
template<class Id> constexpr bool is_commutative(Id) { return false; }
constexpr bool is_commutative(wrap id) { return id == wrap::add || id == wrap::mul; }
constexpr bool is_commutative(rop  id) { return id == rop ::add || id == rop ::mul; }
constexpr bool is_commutative(icmp id) { return id == icmp::  e || id == icmp:: ne; }
constexpr bool is_commutative(rcmp id) { return id == rcmp::  e || id == rcmp:: ne; }
// clang-format off

constexpr bool is_commutative(bit2  id) {
    auto tab = make_truth_table(id);
    return tab[0][1] == tab[1][0];
}

template<class Id>
constexpr bool is_associative(Id id) { return is_commutative(id); }

constexpr bool is_associative(bit2 id) {
    switch (id) {
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

/*
 * Fold
 */

// This code assumes two-complement arithmetic for unsigned operations.
// This is *implementation-defined* but *NOT* *undefined behavior*.

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

template<class T, T, nat_t>
struct Fold {};

template<class Id>
static void commute(Id id, const Def*& a, const Def*& b) {
    if (is_commutative(id)) {
        if (b->isa<Lit>() || (a->gid() > b->gid() && !a->isa<Lit>()))
            std::swap(a, b); // swap lit to left, or smaller gid to left if no lit present
    }
}

/// @attention Note that @p a and @p b are passed by reference as fold also commutes if possible. @sa commute().
template<class Id, Id id, bool isa_wrap = std::is_same_v<Id, wrap>>
static const Def* fold(World& world, const Def* type, const App* callee, const Def*& a, const Def*& b, const Def* dbg) {
    static constexpr int min_w = std::is_same_v<Id, rop> || std::is_same_v<Id, rcmp> ? 16 : 1;
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
        } else {
            width = as_lit(force<Real>(a->type())->arg());
        }

        if (is_int<Id>()) width = *size2bitwidth(width);

        Res res;
        switch (width) {
#define CODE(i)                                                             \
    case i:                                                                 \
        if constexpr (i >= min_w) {                                         \
            if constexpr (isa_wrap)                                         \
                res = Fold<Id, id, i>::run(la->get(), lb->get(), nsw, nuw); \
            else                                                            \
                res = Fold<Id, id, i>::run(la->get(), lb->get());           \
        }                                                                   \
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

template<nat_t w> struct Fold<rop, rop:: add, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) + get<T>(b)); } };
template<nat_t w> struct Fold<rop, rop:: sub, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) - get<T>(b)); } };
template<nat_t w> struct Fold<rop, rop:: mul, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) * get<T>(b)); } };
template<nat_t w> struct Fold<rop, rop:: div, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) / get<T>(b)); } };
template<nat_t w> struct Fold<rop, rop:: rem, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(rem(get<T>(a), get<T>(b))); } };
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

template<rcmp cmp, nat_t w>
struct Fold<rcmp, cmp, w> {
    inline static Res run(u64 a, u64 b) {
        using T = w2r<w>;
        auto x = get<T>(a), y = get<T>(b);
        bool result = false;
        result |= ((cmp & rcmp::u) != rcmp::f) && std::isunordered(x, y);
        result |= ((cmp & rcmp::g) != rcmp::f) && x > y;
        result |= ((cmp & rcmp::l) != rcmp::f) && x < y;
        result |= ((cmp & rcmp::e) != rcmp::f) && x == y;
        return result;
    }
};

// clang-format off
template<conv id, nat_t, nat_t> struct FoldConv {};
template<nat_t dw, nat_t sw> struct FoldConv<conv::s2s, dw, sw> { static Res run(u64 src) { return w2s<dw>(get<w2s<sw>>(src)); } };
template<nat_t dw, nat_t sw> struct FoldConv<conv::u2u, dw, sw> { static Res run(u64 src) { return w2u<dw>(get<w2u<sw>>(src)); } };
template<nat_t dw, nat_t sw> struct FoldConv<conv::s2r, dw, sw> { static Res run(u64 src) { return w2r<dw>(get<w2s<sw>>(src)); } };
template<nat_t dw, nat_t sw> struct FoldConv<conv::u2r, dw, sw> { static Res run(u64 src) { return w2r<dw>(get<w2u<sw>>(src)); } };
template<nat_t dw, nat_t sw> struct FoldConv<conv::r2s, dw, sw> { static Res run(u64 src) { return w2s<dw>(get<w2r<sw>>(src)); } };
template<nat_t dw, nat_t sw> struct FoldConv<conv::r2u, dw, sw> { static Res run(u64 src) { return w2u<dw>(get<w2r<sw>>(src)); } };
template<nat_t dw, nat_t sw> struct FoldConv<conv::r2r, dw, sw> { static Res run(u64 src) { return w2r<dw>(get<w2r<sw>>(src)); } };
// clang-format on

template<class Id>
static const Def* merge_cmps(std::array<std::array<u64, 2>, 2> tab, const Def* a, const Def* b, const Def* dbg) {
    static_assert(sizeof(sub_t) == 1, "if this ever changes, please adjust the logic below");
    static constexpr size_t num_bits = std::bit_width(Axiom::Num<Id> - 1_u64);

    auto a_cmp  = match<Id>(a);
    auto b_cmp  = match<Id>(b);
    auto& world = a->world();

    if (a_cmp && b_cmp && a_cmp->args() == b_cmp->args()) {
        // push sub bits of a_cmp and b_cmp through truth table
        sub_t res   = 0;
        sub_t a_sub = a_cmp.sub();
        sub_t b_sub = b_cmp.sub();
        for (size_t i = 0; i != num_bits; ++i, res >>= 1, a_sub >>= 1, b_sub >>= 1)
            res |= tab[a_sub & 1][b_sub & 1] << 7_u8;
        res >>= (7_u8 - u8(num_bits));

        if constexpr (std::is_same_v<Id, rcmp>)
            return op(rcmp(res), /*rmode*/ a_cmp->decurry()->arg(0), a_cmp->arg(0), a_cmp->arg(1), dbg);
        else
            return world.call(icmp(Axiom::Base<icmp> | res), {a_cmp->arg(0), a_cmp->arg(1)}, dbg);
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

    if constexpr (std::is_same_v<Id, rop>) {
        // build rmode for all new ops by using the least upper bound of all involved apps
        nat_t rmode     = RMode::bot;
        auto check_mode = [&](const App* app) {
            auto app_m = isa_lit(app->arg(0));
            if (!app_m || !(*app_m & RMode::reassoc)) return false;
            rmode &= *app_m; // least upper bound
            return true;
        };

        if (!check_mode(ab)) return nullptr;
        if (lx && !check_mode(xy->decurry())) return nullptr;
        if (lz && !check_mode(zw->decurry())) return nullptr;

        make_op = [&](const Def* a, const Def* b) { return op(id, rmode, a, b, dbg); };
    } else if constexpr (std::is_same_v<Id, wrap>) {
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

template<rop id>
const Def* normalize_rop(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto [m, w] = callee->args<2>(isa_lit<nat_t>); // mode and width

    if (auto result = fold<rop, id>(world, type, callee, a, b, dbg)) return result;

    // clang-format off
    // TODO check rmode properly
    if (m && *m == RMode::fast) {
        if (auto la = a->isa<Lit>()) {
            if (la == lit_real(world, *w, 0.0)) {
                switch (id) {
                    case rop::add: return b;    // 0 + b -> b
                    case rop::sub: break;
                    case rop::mul: return la;   // 0 * b -> 0
                    case rop::div: return la;   // 0 / b -> 0
                    case rop::rem: return la;   // 0 % b -> 0
                }
            }

            if (la == lit_real(world, *w, 1.0)) {
                switch (id) {
                    case rop::add: break;
                    case rop::sub: break;
                    case rop::mul: return b;    // 1 * b -> b
                    case rop::div: break;
                    case rop::rem: break;
                }
            }
        }

        if (auto lb = b->isa<Lit>()) {
            if (lb == lit_real(world, *w, 0.0)) {
                switch (id) {
                    case rop::sub: return a;    // a - 0 -> a
                    case rop::div: break;
                    case rop::rem: break;
                    default: unreachable();
                    // add, mul are commutative, the literal has been normalized to the left
                }
            }
        }

        if (a == b) {
            switch (id) {
                case rop::add: return core::op(rop::mul, lit_real(world, *w, 2.0), a, dbg); // a + a -> 2 * a
                case rop::sub: return lit_real(world, *w, 0.0);                             // a - a -> 0
                case rop::mul: break;
                case rop::div: return lit_real(world, *w, 1.0);                             // a / a -> 1
                case rop::rem: break;
            }
        }
    }
    // clang-format on

    if (auto res = reassociate<rop>(id, world, callee, a, b, dbg)) return res;

    return world.raw_app(callee, {a, b}, dbg);
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

template<rcmp id>
const Def* normalize_rcmp(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();

    if (auto result = fold<rcmp, id>(world, type, callee, a, b, dbg)) return result;
    if (id == rcmp::f) return world.lit_ff();
    if (id == rcmp::t) return world.lit_tt();

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

template<bit2 id>
const Def* normalize_bit2(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto w      = isa_lit(callee->arg());

    commute(id, a, b);

    auto tab = make_truth_table(id);
    if (auto res = merge_cmps<icmp>(tab, a, b, dbg)) return res;
    if (auto res = merge_cmps<rcmp>(tab, a, b, dbg)) return res;

    auto la = isa_lit(a);
    auto lb = isa_lit(b);

    // clang-format off
    switch (id) {
        case bit2::    f: return world.lit_idx(*w,        0);
        case bit2::    t: return world.lit_idx(*w, *w-1_u64);
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
            case bit2::_and: return world.lit_idx    (*w,   *la &  *lb);
            case bit2:: _or: return world.lit_idx    (*w,   *la |  *lb);
            case bit2::_xor: return world.lit_idx    (*w,   *la ^  *lb);
            case bit2::nand: return world.lit_idx_mod(*w, ~(*la &  *lb));
            case bit2:: nor: return world.lit_idx_mod(*w, ~(*la |  *lb));
            case bit2::nxor: return world.lit_idx_mod(*w, ~(*la ^  *lb));
            case bit2:: iff: return world.lit_idx_mod(*w, ~ *la |  *lb);
            case bit2::niff: return world.lit_idx    (*w,   *la & ~*lb);
            default: unreachable();
        }
    }

    auto unary = [&](bool x, bool y, const Def* a) -> const Def* {
        if (!x && !y) return world.lit_idx(*w, 0);
        if ( x &&  y) return world.lit_idx(*w, *w-1_u64);
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
        if (la == world.lit_idx(*w, 0)) {
            switch (id) {
                case shr::a: return la;
                case shr::l: return la;
            }
        }
    }

    if (auto lb = b->isa<Lit>()) {
        if (lb == world.lit_idx(*w, 0)) {
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
        if (la == world.lit_idx(*w, 0)) {
            switch (id) {
                case wrap::add: return b;    // 0  + b -> b
                case wrap::sub: break;
                case wrap::mul: return la;   // 0  * b -> 0
                case wrap::shl: return la;   // 0 << b -> 0
            }
        }

        if (la == world.lit_idx(*w, 1)) {
            switch (id) {
                case wrap::add: break;
                case wrap::sub: break;
                case wrap::mul: return b;    // 1  * b -> b
                case wrap::shl: break;
            }
        }
    }

    if (auto lb = b->isa<Lit>()) {
        if (lb == world.lit_idx(*w, 0)) {
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
            case wrap::add: return op(wrap::mul, *m, world.lit_idx(*w, 2), a, dbg); // a + a -> 2 * a
            case wrap::sub: return world.lit_idx(*w, 0);                                  // a - a -> 0
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
    auto w           = isa_lit(callee->arg());
    type             = type->as<Sigma>()->op(1); // peel of actual type
    auto make_res    = [&, mem = mem](const Def* res) { return world.tuple({mem, res}, dbg); };

    if (auto result = fold<div, id>(world, type, callee, a, b, dbg)) return make_res(result);

    if (auto la = a->isa<Lit>()) {
        if (la == world.lit_idx(*w, 0)) return make_res(la); // 0 / b -> 0 and 0 % b -> 0
    }

    if (auto lb = b->isa<Lit>()) {
        if (lb == world.lit_idx(*w, 0)) return make_res(world.bot(type)); // a / 0 -> ⊥ and a % 0 -> ⊥

        if (lb == world.lit_idx(*w, 1)) {
            switch (id) {
                case div::sdiv: return make_res(a);                    // a / 1 -> a
                case div::udiv: return make_res(a);                    // a / 1 -> a
                case div::srem: return make_res(world.lit_idx(*w, 0)); // a % 1 -> 0
                case div::urem: return make_res(world.lit_idx(*w, 0)); // a % 1 -> 0
            }
        }
    }

    if (a == b) {
        switch (id) {
            case div::sdiv: return make_res(world.lit_idx(*w, 1)); // a / a -> 1
            case div::udiv: return make_res(world.lit_idx(*w, 1)); // a / a -> 1
            case div::srem: return make_res(world.lit_idx(*w, 0)); // a % a -> 0
            case div::urem: return make_res(world.lit_idx(*w, 0)); // a % a -> 0
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

template<nat_t min_sw, nat_t min_dw, conv id>
static const Def* fold_conv(const Def* dst_type, const App* callee, const Def* src, const Def* dbg) {
    auto& world = dst_type->world();
    if (src->isa<Bot>()) return world.bot(dst_type, dbg);

    auto [lit_dw, lit_sw] = callee->args<2>(isa_lit<nat_t>);
    auto lit_src          = src->isa<Lit>();
    if (lit_src && lit_dw && lit_sw) {
        if (id == conv::u2u) {
            if (*lit_dw == 0) return world.lit(dst_type, as_lit(lit_src));
            return world.lit(dst_type, as_lit(lit_src) % *lit_dw);
        }

        if (Idx::size(src->type())) *lit_sw = *size2bitwidth(*lit_sw);
        if (Idx::size(dst_type)) *lit_dw = *size2bitwidth(*lit_dw);

        Res res;
#define CODE(sw, dw)                                                                                 \
    else if (*lit_dw == dw && *lit_sw == sw) {                                                       \
        if constexpr (dw >= min_dw && sw >= min_sw) res = FoldConv<id, dw, sw>::run(lit_src->get()); \
    }
        if (false) {}
        TABLE(CODE)
#undef CODE
        if (res) return world.lit(dst_type, *res, dbg);
        return world.bot(dst_type, dbg);
    }

    return nullptr;
}

template<conv id>
const Def* normalize_conv(const Def* dst_type, const Def* c, const Def* src, const Def* dbg) {
    auto& world = dst_type->world();
    auto callee = c->as<App>();

    static constexpr auto min_sw = id == conv::r2s || id == conv::r2u || id == conv::r2r ? 16 : 1;
    static constexpr auto min_dw = id == conv::s2r || id == conv::u2r || id == conv::r2r ? 16 : 1;
    if (auto result = fold_conv<min_sw, min_dw, id>(dst_type, callee, src, dbg)) return result;

    auto [dw, sw] = callee->args<2>(isa_lit<nat_t>);
    if (sw == dw && dst_type == src->type()) return src;

    if constexpr (id == conv::s2s) {
        if (sw && dw && *sw < *dw) return core::op(conv::u2u, dst_type, src, dbg);
    }

    return world.raw_app(callee, src, dbg);
}

const Def* normalize_bitcast(const Def* dst_type, const Def* callee, const Def* src, const Def* dbg) {
    auto& world = dst_type->world();

    if (src->isa<Bot>()) return world.bot(dst_type);
    if (src->type() == dst_type) return src;

    if (auto other = match<bitcast>(src))
        return other->arg()->type() == dst_type ? other->arg() : op_bitcast(dst_type, other->arg(), dbg);

    if (auto lit = src->isa<Lit>()) {
        if (dst_type->isa<Nat>()) return world.lit(dst_type, lit->get(), dbg);
        if (Idx::size(dst_type)) return world.lit(dst_type, lit->get(), dbg);
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
    } else if (auto real = match<Real>(type)) {
        if (auto w = isa_lit(real->arg())) {
            switch (*w) {
                case 16: return world.lit_nat(2);
                case 32: return world.lit_nat(4);
                case 64: return world.lit_nat(8);
                default: unreachable();
            }
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
        auto align = core::op(trait::align, arr->body());
        if constexpr (id == trait::align) return align;

        if (auto b = isa_lit(core::op(trait::size, arr->body()))) {
            auto i64_t = world.type_int(64);
            auto s     = op_bitcast(i64_t, arr->shape());
            auto mul   = core::op(wrap::mul, WMode::nsw | WMode::nuw, world.lit_idx(i64_t, *b), s);
            return op_bitcast(world.type_nat(), mul);
        }
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
