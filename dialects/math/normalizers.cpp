#include <type_traits>

#include "dialects/core/core.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::math {

template<class T>
static T get(u64 u) {
    return thorin::bitcast<T>(u);
}

/*
 * is_commutative & is_associative
 */

// clang-format off
template<class Id> constexpr bool is_commutative(Id) { return false; }
constexpr bool is_commutative(arith id) { return id == arith ::add || id == arith::mul; }
constexpr bool is_commutative(cmp   id) { return id == cmp   ::e   || id == cmp  ::ne ; }
// clang-format off

template<class Id>
constexpr bool is_associative(Id id) { return is_commutative(id); }

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
template<class Id, Id id>
static const Def* fold2(World& world, const Def* type, const Def*& a, const Def*& b, const Def* dbg) {
    auto la = a->isa<Lit>(), lb = b->isa<Lit>();

    if (a->isa<Bot>() || b->isa<Bot>()) return world.bot(type, dbg);

    if (la && lb) {
        nat_t width = *isa_f(a->type());
        Res res;
        switch (width) {
#define CODE(i)                                                   \
            case i:                                               \
                res = Fold<Id, id, i>::run(la->get(), lb->get()); \
                break;
            THORIN_16_32_64(CODE)
#undef CODE
            default: unreachable();
        }

        return res ? world.lit(type, *res, dbg) : world.bot(type, dbg);
    }

    commute(id, a, b);
    return nullptr;
}

// clang-format off
template<nat_t w> struct Fold<arith, arith::add, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) + get<T>(b)); } };
template<nat_t w> struct Fold<arith, arith::sub, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) - get<T>(b)); } };
template<nat_t w> struct Fold<arith, arith::mul, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) * get<T>(b)); } };
template<nat_t w> struct Fold<arith, arith::div, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(get<T>(a) / get<T>(b)); } };
template<nat_t w> struct Fold<arith, arith::rem, w> { static Res run(u64 a, u64 b) { using T = w2r<w>; return T(rem(get<T>(a), get<T>(b))); } };
// clang-format on

template<cmp r, nat_t w>
struct Fold<cmp, r, w> {
    inline static Res run(u64 a, u64 b) {
        using T = w2r<w>;
        auto x = get<T>(a), y = get<T>(b);
        bool result = false;
        result |= ((r & cmp::u) != cmp::f) && std::isunordered(x, y);
        result |= ((r & cmp::g) != cmp::f) && x > y;
        result |= ((r & cmp::l) != cmp::f) && x < y;
        result |= ((r & cmp::e) != cmp::f) && x == y;
        return result;
    }
};

// clang-format off
template<conv id, nat_t, nat_t> struct FoldConv {};
template<nat_t dw, nat_t sw> struct FoldConv<conv::s2f, dw, sw> { static Res run(u64 src) { return w2r<dw>(get<w2s<sw>>(src)); } };
template<nat_t dw, nat_t sw> struct FoldConv<conv::u2f, dw, sw> { static Res run(u64 src) { return w2r<dw>(get<w2u<sw>>(src)); } };
template<nat_t dw, nat_t sw> struct FoldConv<conv::f2s, dw, sw> { static Res run(u64 src) { return w2s<dw>(get<w2r<sw>>(src)); } };
template<nat_t dw, nat_t sw> struct FoldConv<conv::f2u, dw, sw> { static Res run(u64 src) { return w2u<dw>(get<w2r<sw>>(src)); } };
template<nat_t dw, nat_t sw> struct FoldConv<conv::f2f, dw, sw> { static Res run(u64 src) { return w2r<dw>(get<w2r<sw>>(src)); } };
// clang-format on

template<class Id>
static const Def* merge_cmps(std::array<std::array<u64, 2>, 2> tab, const Def* a, const Def* b, const Def* dbg) {
    static_assert(sizeof(sub_t) == 1, "if this ever changes, please adjust the logic below");
    static constexpr size_t num_bits = std::bit_width(Axiom::Num<Id> - 1_u64);

    auto a_cmp = match<Id>(a);
    auto b_cmp = match<Id>(b);

    if (a_cmp && b_cmp && a_cmp->args() == b_cmp->args()) {
        // push sub bits of a_cmp and b_cmp through truth table
        sub_t res   = 0;
        sub_t a_sub = a_cmp.sub();
        sub_t b_sub = b_cmp.sub();
        for (size_t i = 0; i != num_bits; ++i, res >>= 1, a_sub >>= 1, b_sub >>= 1)
            res |= tab[a_sub & 1][b_sub & 1] << 7_u8;
        res >>= (7_u8 - u8(num_bits));

        return op(cmp(res), /*rmode*/ a_cmp->decurry()->arg(), a_cmp->arg(0), a_cmp->arg(1), dbg);
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
    auto [m, w] = callee->args<2>(isa_lit<nat_t>); // mode and width

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

// clang-format off
#define TABLE(m) m( 1,  1) m( 1,  8) m( 1, 16) m( 1, 32) m( 1, 64) \
                 m( 8,  1) m( 8,  8) m( 8, 16) m( 8, 32) m( 8, 64) \
                 m(16,  1) m(16,  8) m(16, 16) m(16, 32) m(16, 64) \
                 m(32,  1) m(32,  8) m(32, 16) m(32, 32) m(32, 64) \
                 m(64,  1) m(64,  8) m(64, 16) m(64, 32) m(64, 64)
// clang-format on

#if 0
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
#    define CODE(sw, dw)                                                                                 \
        else if (*lit_dw == dw && *lit_sw == sw) {                                                       \
            if constexpr (dw >= min_dw && sw >= min_sw) res = FoldConv<id, dw, sw>::run(lit_src->get()); \
        }
        if (false) {}
        TABLE(CODE)
#    undef CODE
        if (res) return world.lit(dst_type, *res, dbg);
        return world.bot(dst_type, dbg);
    }

    return nullptr;
}
#endif

template<conv id>
const Def* normalize_conv(const Def* dst_type, const Def* c, const Def* src, const Def* dbg) {
    auto& world = dst_type->world();
    auto callee = c->as<App>();
#if 0

    static constexpr auto min_sw = id == conv::r2s || id == conv::r2u || id == conv::r2r ? 16 : 1;
    static constexpr auto min_dw = id == conv::s2r || id == conv::u2r || id == conv::r2r ? 16 : 1;
    if (auto result = fold_conv<min_sw, min_dw, id>(dst_type, callee, src, dbg)) return result;

    auto [dw, sw] = callee->args<2>(isa_lit<nat_t>);
    if (sw == dw && dst_type == src->type()) return src;

    if constexpr (id == conv::s2s) {
        if (sw && dw && *sw < *dw) return math::op(conv::u2u, dst_type, src, dbg);
    }
#endif

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
    } else if (auto w = isa_f(type)) {
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
            auto a = isa_lit(math::op(trait::align, t));
            auto s = isa_lit(math::op(trait::size, t));
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
        auto align = math::op(trait::align, arr->body());
        if constexpr (id == trait::align) return align;
        if (auto b = math::op(trait::size, arr->body())->isa<Lit>()) return core::op(core::nop::mul, arr->shape(), b);
    }

out:
    return world.raw_app(callee, type, dbg);
}

THORIN_math_NORMALIZER_IMPL

} // namespace thorin::math
