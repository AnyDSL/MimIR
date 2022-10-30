#include <type_traits>

#include "thorin/normalize.h"

#include "dialects/math/math.h"

namespace thorin::normalize {

// clang-format off
constexpr bool is_commutative(math::x       ) { return true; }
constexpr bool is_commutative(math::arith id) { return id == math::arith ::add || id == math::arith::mul; }
constexpr bool is_commutative(math::cmp   id) { return id == math::cmp   ::e   || id == math::cmp  ::ne ; }
// clang-format off

// TODO move to normalize.h
/// Swap Lit to left - or smaller gid, if no lit present.
template<class Id>
static void commute(Id id, const Def*& a, const Def*& b) {
    if (is_commutative(id)) {
        if (b->isa<Lit>() || (a->gid() > b->gid() && !a->isa<Lit>()))
            std::swap(a, b);
    }
}

}

using namespace thorin::normalize;

namespace thorin::math {

template<class T, T, nat_t>
struct Fold {};

// clang-format off
template<nat_t w> struct Fold<arith, arith::add, w> { static Res run(u64 a, u64 b) { using T = w2f<w>; return T(get<T>(a) + get<T>(b)); } };
template<nat_t w> struct Fold<arith, arith::sub, w> { static Res run(u64 a, u64 b) { using T = w2f<w>; return T(get<T>(a) - get<T>(b)); } };
template<nat_t w> struct Fold<arith, arith::mul, w> { static Res run(u64 a, u64 b) { using T = w2f<w>; return T(get<T>(a) * get<T>(b)); } };
template<nat_t w> struct Fold<arith, arith::div, w> { static Res run(u64 a, u64 b) { using T = w2f<w>; return T(get<T>(a) / get<T>(b)); } };
template<nat_t w> struct Fold<arith, arith::rem, w> { static Res run(u64 a, u64 b) { using T = w2f<w>; return T(rem(get<T>(a), get<T>(b))); } };
// clang-format on

template<cmp r, nat_t w>
struct Fold<cmp, r, w> {
    inline static Res run(u64 a, u64 b) {
        using T = w2f<w>;
        auto x = get<T>(a), y = get<T>(b);
        bool result = false;
        result |= ((r & cmp::u) != cmp::f) && std::isunordered(x, y);
        result |= ((r & cmp::g) != cmp::f) && x > y;
        result |= ((r & cmp::l) != cmp::f) && x < y;
        result |= ((r & cmp::e) != cmp::f) && x == y;
        return result;
    }
};

// TODO Deal with UB in the context of signed conversions.
// clang-format off
template<conv id, nat_t, nat_t> struct FoldConv {};
template<nat_t sw, nat_t dw> struct FoldConv<conv::s2f, sw, dw> { static Res run(u64 s) { return w2f<dw>(get<w2s<sw>>(s)); } };
template<nat_t sw, nat_t dw> struct FoldConv<conv::u2f, sw, dw> { static Res run(u64 s) { return w2f<dw>(get<w2u<sw>>(s)); } };
template<nat_t sw, nat_t dw> struct FoldConv<conv::f2s, sw, dw> { static Res run(u64 s) { return w2s<dw>(get<w2f<sw>>(s)); } };
template<nat_t sw, nat_t dw> struct FoldConv<conv::f2u, sw, dw> { static Res run(u64 s) { return w2u<dw>(get<w2f<sw>>(s)); } };
template<nat_t sw, nat_t dw> struct FoldConv<conv::f2f, sw, dw> { static Res run(u64 s) { return w2f<dw>(get<w2f<sw>>(s)); } };
// clang-format on

/// @attention Note that @p a and @p b are passed by reference as fold also commutes if possible. @sa commute().
template<class Id, Id id>
static const Def* fold2(World& world, const Def* type, const Def*& a, const Def*& b, const Def* dbg) {
    auto la = a->isa<Lit>(), lb = b->isa<Lit>();

    if (a->isa<Bot>() || b->isa<Bot>()) return world.bot(type, dbg);

    if (la && lb) {
        nat_t width = *isa_f(a->type());
        Res res;
        switch (width) {
#define CODE(i) \
    case i: res = Fold<Id, id, i>::run(la->get(), lb->get()); break;
            THORIN_16_32_64(CODE)
#undef CODE
            default: unreachable();
        }

        return res ? world.lit(type, *res, dbg) : world.bot(type, dbg);
    }

    commute(id, a, b);
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

    // build rmode for all new ops by using the least upper bound of all involved apps
    nat_t rmode     = Mode::bot;
    auto check_mode = [&](const App* app) {
        auto app_m = isa_lit(app->arg(0));
        if (!app_m || !(*app_m & Mode::reassoc)) return false;
        rmode &= *app_m; // least upper bound
        return true;
    };

    if (!check_mode(ab)) return nullptr;
    if (lx && !check_mode(xy->decurry())) return nullptr;
    if (lz && !check_mode(zw->decurry())) return nullptr;

    make_op = [&](const Def* a, const Def* b) { return op(id, rmode, a, b, dbg); };

    if (la && lz) return make_op(make_op(la, lz), w);             // (1)
    if (lx && lz) return make_op(make_op(lx, lz), make_op(y, w)); // (2)
    if (lz) return make_op(lz, make_op(a, w));                    // (3)
    if (lx) return make_op(lx, make_op(y, b));                    // (4)

    return nullptr;
}

template<arith id>
const Def* normalize_arith(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto m      = isa_lit(callee->decurry()->arg());
    auto w      = isa_f(a->type());

    if (auto result = fold2<arith, id>(world, type, a, b, dbg)) return result;

    // clang-format off
    // TODO check rmode properly
    if (m && *m == Mode::fast) {
        if (auto la = a->isa<Lit>()) {
            if (la == lit_f(world, *w, 0.0)) {
                switch (id) {
                    case arith::add: return b;    // 0 + b -> b
                    case arith::sub: break;
                    case arith::mul: return la;   // 0 * b -> 0
                    case arith::div: return la;   // 0 / b -> 0
                    case arith::rem: return la;   // 0 % b -> 0
                }
            }

            if (la == lit_f(world, *w, 1.0)) {
                switch (id) {
                    case arith::add: break;
                    case arith::sub: break;
                    case arith::mul: return b;    // 1 * b -> b
                    case arith::div: break;
                    case arith::rem: break;
                }
            }
        }

        if (auto lb = b->isa<Lit>()) {
            if (lb == lit_f(world, *w, 0.0)) {
                switch (id) {
                    case arith::sub: return a;    // a - 0 -> a
                    case arith::div: break;
                    case arith::rem: break;
                    default: unreachable();
                    // add, mul are commutative, the literal has been normalized to the left
                }
            }
        }

        if (a == b) {
            switch (id) {
                case arith::add: return math::op(arith::mul, lit_f(world, *w, 2.0), a, dbg); // a + a -> 2 * a
                case arith::sub: return lit_f(world, *w, 0.0);                             // a - a -> 0
                case arith::mul: break;
                case arith::div: return lit_f(world, *w, 1.0);                             // a / a -> 1
                case arith::rem: break;
            }
        }
    }
    // clang-format on

    if (auto res = reassociate<arith>(id, world, callee, a, b, dbg)) return res;

    return world.raw_app(callee, {a, b}, dbg);
}

template<x id>
const Def* normalize_x(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    return type->world().raw_app(c, arg, dbg);
}

template<tri id>
const Def* normalize_tri(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    return type->world().raw_app(c, arg, dbg);
}

const Def* normalize_pow(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    return type->world().raw_app(c, arg, dbg);
}

template<rt id>
const Def* normalize_rt(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    return type->world().raw_app(c, arg, dbg);
}

template<exp id>
const Def* normalize_exp(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    return type->world().raw_app(c, arg, dbg);
}

template<er id>
const Def* normalize_er(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    return type->world().raw_app(c, arg, dbg);
}

template<gamma id>
const Def* normalize_gamma(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    return type->world().raw_app(c, arg, dbg);
}

template<cmp id>
const Def* normalize_cmp(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();

    if (auto result = fold2<cmp, id>(world, type, a, b, dbg)) return result;
    if (id == cmp::f) return world.lit_ff();
    if (id == cmp::t) return world.lit_tt();

    return world.raw_app(callee, {a, b}, dbg);
}

template<conv id>
const Def* normalize_conv(const Def* dst_ty, const Def* c, const Def* x, const Def* dbg) {
    auto& world = dst_ty->world();
    auto callee = c->as<App>();
    auto s_ty   = x->type()->as<App>();
    auto d_ty   = dst_ty->as<App>();
    auto s      = s_ty->arg();
    auto d      = d_ty->arg();
    auto ls     = isa_lit(s);
    auto ld     = isa_lit(d);

    if (s_ty == d_ty) return x;
    if (x->isa<Bot>()) return world.bot(d_ty, dbg);

    if (auto l = isa_lit(x); l && ls && ld) {
        constexpr bool sf     = id == conv::f2f || id == conv::f2s || id == conv::f2u;
        constexpr bool df     = id == conv::f2f || id == conv::s2f || id == conv::u2f;
        constexpr nat_t min_s = sf ? 16 : 1;
        constexpr nat_t min_d = df ? 16 : 1;
        auto sw               = sf ? isa_f(s_ty) : size2bitwidth(*ls);
        auto dw               = df ? isa_f(d_ty) : size2bitwidth(*ld);

        if (sw && dw) {
            Res res;
            // clang-format off
            if (false) {}
#define M(S, D) \
            else if (S == *sw && D == *dw) { \
                if constexpr (S >= min_s && D >= min_d) \
                    res = FoldConv<id, S, D>::run(*l); \
                else \
                    goto out; \
            }
            M( 1,  1) M( 1,  8) M( 1, 16) M( 1, 32) M( 1, 64)
            M( 8,  1) M( 8,  8) M( 8, 16) M( 8, 32) M( 8, 64)
            M(16,  1) M(16,  8) M(16, 16) M(16, 32) M(16, 64)
            M(32,  1) M(32,  8) M(32, 16) M(32, 32) M(32, 64)
            M(64,  1) M(64,  8) M(64, 16) M(64, 32) M(64, 64)

            else unreachable();
            // clang-format on

            if (res) return world.lit(d_ty, *res, dbg);
            assert(false && "TODO");
            return world.bot(d_ty, dbg);
        }
    }
out:
    return world.raw_app(callee, x, dbg);
}

// TODO I guess we can do that with C++20 <bit>
inline u64 pad(u64 offset, u64 align) {
    auto mod = offset % align;
    if (mod != 0) offset += align - mod;
    return offset;
}

THORIN_math_NORMALIZER_IMPL

} // namespace thorin::math
